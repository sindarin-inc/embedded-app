#include "UI/KindleReadableScreen.hpp"

#include <algorithm> // Required for std::min/max
#include <cinttypes>
#include <sstream> // Required for std::istringstream
#include <vector>  // Required for std::vector

#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEManager.hpp"
#endif
#include "Misc/StringUtil.hpp"
#include "Models/Readable.hpp"
#include "UI/Fonts/Fonts.hpp"
#include "sindarin-debug.h"

// Helper function to get N words
auto GetNWords(const std::string &text, int n, bool fromStart) -> std::string {
    std::string result;
    std::istringstream iss(text);
    std::string word;
    std::vector<std::string> words;
    while (iss >> word) {
        words.push_back(word);
    }

    if (words.empty()) {
        return "";
    }

    if (fromStart) {
        for (int i = 0; i < std::min(static_cast<int>(words.size()), n); ++i) {
            result += words[i] + " ";
        }
    } else {
        for (int i = std::max(0, static_cast<int>(words.size()) - n); i < words.size(); ++i) {
            result += words[i] + " ";
        }
    }
    // Trim trailing space
    if (!result.empty()) {
        result.pop_back();
    }
    return result;
}

KindleReadableScreen::KindleReadableScreen(const std::shared_ptr<RendererStream> &stream,
                                           DisplaySystem &display,
                                           const std::shared_ptr<KindleBook> &kindleBook,
                                           PreferencesStore &prefs, Telemetry &telemetry,
                                           FileSystem &fileSystem,
                                           LightingController &lightingController,
                                           LocalStore &localStore, Font *font)
    : EinkView(display), stream_(stream),
      renderer_(display, GetTextRectFromPrefs(display, prefs), font), prefs_(prefs),
      telemetry_(telemetry), fileSystem_(fileSystem), lightingController_(lightingController),
      localStore_(localStore), kindleBook_(kindleBook), loadingMessageView_(display, 5),
      errorMessageView_(display, 5), delayedMessageView_(display, 5) {

    // Initialize state variables

    // Start with this false so first call can be a preview
    log_i("Created KindleReadableScreen for book: %s by %s", kindleBook->title.c_str(),
          kindleBook->author.c_str());

    // Since we don't have a standard Readable for Kindle books,
    // we need to set up some of our state differently
    firstKindleLoad_ = true;

    // Configure the renderer to ignore EOF for streaming Kindle content
    renderer_.setIgnoreEOF(true);

    // Set up loading message
    loadingMessageView_.setText("Connecting to your Kindle...");
    loadingMessageView_.setFont(font);
    loadingMessageView_.alignCenter = true;
    loadingMessageView_.dynamicWidth = false; // Ensure we use the full width of our frame

    // Set up error message view
    errorMessageView_.setFont(font);
    errorMessageView_.setText(""); // Initialize with empty text
    errorMessageView_.alignCenter = true;
    errorMessageView_.setHidden(true); // Start hidden until needed

    // Set up delayed message view (for "This can take up to 30 seconds")
    delayedMessageView_.setFont(font);
    delayedMessageView_.setText("");
    delayedMessageView_.alignCenter = true;
    delayedMessageView_.setHidden(true); // Start hidden until needed

    // Initialize network connection
    setState(State::CONNECTING);

    // Initialize the popup menu
    auto popupNavigationFactory = [this]() {
        std::shared_ptr<std::vector<std::shared_ptr<MenuItem>>> menuItems =
            std::make_shared<std::vector<std::shared_ptr<MenuItem>>>();

        menuItems->emplace_back(std::make_shared<MenuItemSingleLine>("Back to Text", [this]() {
            display_.clearDisplay();
            this->setCurrentScreen(nullptr);
            setNeedsRerender();
        }));

        menuItems->emplace_back(
            std::make_shared<MenuItemSingleLine>("Main Menu", [this]() { dismissScreen(); }));

        return menuItems;
    };

    // Create a readable-like object for the popup menu to use
    auto readablePlaceholder = std::make_shared<Readable>();
    readablePlaceholder->title = kindleBook_->title;

    // Add authors from kindle book to readable
    Author author;
    author.name = kindleBook_->author;
    readablePlaceholder->authors.push_back(author);

    popupMenuScreen_ = std::make_shared<NewReadablePopupMenuScreen>(
        display, prefs_, fileSystem_, readablePlaceholder, lightingController_, renderer_,
        telemetry_, localStore_, stream_);

    popupMenuScreen_->addTab("Popup", popupNavigationFactory, 8);
    popupMenuScreen_->populateMenuView();
}

void KindleReadableScreen::render(DisplaySystem &display) {
    // If we have a popup menu or other screen, let it handle rendering
    if (currentScreen_) {
        log_d("Letting current screen (a popup menu) render KindleReadableScreen, state: %d",
              static_cast<int>(state_));
        currentScreen_->render(display);
        return;
    }

    if (state_ == State::DISPLAY_CONTENT || state_ == State::LOADING_BACKGROUND) {
        log_d("Rendering KindleReadableScreen, state: %d", static_cast<int>(state_));
        // Set up the frame and layout
        setFrame(defaultFrameRect());
        setNeedsLayout();

        // Draw the basic screen (background, etc.)
        draw();

        // Render the current page content
        uint8_t darkPicturePercent = 0;

        // Make sure we have a current place
        if (!currentPlace_) {
            currentPlace_ = renderer_.getCurrentPlace();
            if (!currentPlace_) {
                // Initialize with a starting place if needed
                currentPlace_ = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));
                log_i("Initialized currentPlace_ in render since it was null");
            }
        }

        // Render using our current place and get the next place
        std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(currentPlace_);

        // Log the current and next place offsets for debugging
        if (currentPlace_ && newPlace_) {
            uint32_t currentOffset = currentPlace_->getOffset();
            uint32_t nextOffset = newPlace_->getOffset();
            uint32_t totalSize = renderer_.size();

            // We're logging the position but don't need to use remainingBytes here

            // Calculate and store the page size
            lastRenderedPageSize_ = (nextOffset > currentOffset) ? (nextOffset - currentOffset) : 0;

            log_i("Reading at offset: %" PRIu32 "/%" PRIu32 ", page size: %" PRIu32 " bytes",
                  currentOffset, totalSize, lastRenderedPageSize_);
        }

        // For background loading, we might need to show a message if we're at the end
        uint32_t remainingBytes = 0;
        if (currentPlace_) {
            uint32_t currentOffset = currentPlace_->getOffset();
            uint32_t totalSize = renderer_.size();
            remainingBytes = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;
        }

        // Be careful about how we detect the end for Kindle books
        bool showLoadingMessage = false;
        if (state_ == State::LOADING_BACKGROUND) {
            // For Kindle books, decide based on actual buffer space
            showLoadingMessage = (remainingBytes < bufferThreshold_) ||
                                 (renderer_.atEndOfFile() && remainingBytes < bufferThreshold_ * 2);

            if (showLoadingMessage) {
                // If we need to show the loading message with a rounded rect
                if (showLoadingWithRoundedRect_ && !loadingMessageView_.isHidden()) {
                    // Calculate the bounds of the loading message text
                    int maxWidth = 0;
                    const auto &text = loadingMessageView_.getText();
                    if (!text.empty()) {
                        maxWidth = display_.getTextWidth(text, loadingMessageView_.getFont());
                    }

                    // Add padding around the text
                    const int padding = 12;
                    int lineHeight = loadingMessageView_.getFont()->lineHeight();

                    // Get the position from the parent's layout context
                    SolRect screenRect = layoutContext_.screenFrame();

                    // Calculate perfect center position in the screen
                    int centerY = screenRect.origin.y + screenRect.size.height / 2;

                    // Create rectangle for the background with enough padding for good looks
                    SolRect bgRect = SolRect::zero();
                    if (loadingMessageView_.alignCenter) {
                        bgRect.origin.x =
                            screenRect.origin.x + (screenRect.size.width - maxWidth) / 2 - padding;
                        bgRect.size.width = maxWidth + padding * 2;
                    } else {
                        bgRect.origin.x = screenRect.origin.x - padding;
                        bgRect.size.width = maxWidth + padding * 2;
                    }

                    // Make sure we have enough vertical space in the rectangle
                    // (lineHeight + padding on top and bottom)
                    bgRect.size.height = lineHeight + padding * 2;

                    // Position background rect vertically centered on the screen
                    bgRect.origin.y = centerY - bgRect.size.height / 2;

                    // Now position the loadingMessageView at a precise position that's centered
                    // inside the bgRect The loading frame should be centered inside the background
                    // rectangle
                    SolRect loadingFrame = SolRect::zero();
                    loadingFrame.origin.x = screenRect.origin.x;
                    loadingFrame.origin.y = bgRect.origin.y + (bgRect.size.height - lineHeight) / 2;
                    loadingFrame.size.width = screenRect.size.width;
                    loadingFrame.size.height = lineHeight;

                    // Reposition the loading message view to be exactly centered
                    loadingMessageView_.setFrame(loadingFrame);

                    // Draw filled rounded rectangle with a white fill and black outline
                    // First fill with white background
                    display_.fillRoundRect(bgRect.origin.x, bgRect.origin.y, bgRect.size.width,
                                           bgRect.size.height, 10,
                                           display_.bg); // Corner radius 10, white background

                    // Then draw black outline
                    display_.drawRoundRect(bgRect.origin.x, bgRect.origin.y, bgRect.size.width,
                                           bgRect.size.height, 10,
                                           display_.fg); // Corner radius 10, black outline

                    // Use normal text colors (black text) with white background
                    // No need to save or swap colors

                    // Draw the loading message with normal colors
                    // Log pre-draw state
                    log_i("Drawing message: '%s'", loadingMessageView_.getText().c_str());

                    // Now manually draw the text in the exact center of the rounded rect
                    int textWidth = display_.getTextWidth(loadingMessageView_.getText(),
                                                          loadingMessageView_.getFont());
                    int x = bgRect.origin.x + (bgRect.size.width - textWidth) / 2;
                    int y = bgRect.origin.y +
                            (bgRect.size.height - loadingMessageView_.getFont()->lineHeight()) / 2;

                    // Log the exact drawing position
                    log_i("Drawing text manually at x=%d, y=%d (text width=%d, rect width=%d)", x,
                          y, textWidth, bgRect.size.width);

                    // Draw the text manually at the calculated center position
                    display_.drawSingleLineOfText(text, x, y, false, loadingMessageView_.getFont());
                } else {
                    // Draw normally without rounded rect
                    loadingMessageView_.draw();
                }
            }
        }

        // Update the display
        if (firstKindleLoad_) {
            log_w("RENDER: Updating display to full on first Kindle book load");
            display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
            firstKindleLoad_ = false;
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        }
        return;
    }

    // For other states (connecting, loading, error), we handle custom rendering
    setNeedsLayout();
    setFrame(defaultFrameRect());

    // Draw the basic screen (background, etc.)
    draw();

    // Additional rendering based on state
    switch (state_) {
    case State::CONNECTING:
    case State::LOADING_CONTENT:
        loadingMessageView_.draw();
        break;

    case State::ERROR:
        // Ensure display is clear before showing error message
        display_.clearFrameBuffer();

        // Keep the loading message visible as it contains our error text
        loadingMessageView_.setHidden(false);
        loadingMessageView_.draw();

        // Also draw error message as backup
        errorMessageView_.setHidden(false);
        errorMessageView_.draw();

        log_i("Drawing both loading message and error message views for error state");
        break;

    default:
        break;
    }

    // Handle display updates
    if (firstKindleLoad_) {
        log_w("RENDER: Updating display to full on first Kindle book load");
        display.updateDisplay(DisplaySystem::UpdateType::FULL, true);
        firstKindleLoad_ = false;
    } else if (state_ == State::CONNECTING || state_ == State::LOADING_CONTENT ||
               state_ == State::ERROR) {
        log_w("RENDER: Updating display to full");
        display.updateDisplay(DisplaySystem::UpdateType::FULL, true);
    }
}

void KindleReadableScreen::processCommand(RemoteCommand command) {
    // If we have a popup menu or other screen, delegate command processing to it
    if (currentScreen_) {
        // Save a pointer to the current screen in case it gets reset by a command
        auto screen = currentScreen_;
        screen->processCommand(command);
        // Check if we need to rerender after the command
        if (currentScreen_ && currentScreen_->needsRerender()) {
            setNeedsRerender();
        }
        return;
    }

    // If in an error state, ANY button press dismisses immediately
    if (state_ == State::ERROR) {
        log_i("Button pressed while in ERROR state, immediately dismissing screen and cancelling "
              "auto-dismiss");
        // The dismissScreen method will handle cancelling the auto-dismiss task
        dismissScreen();
        return;
    }

    // If connecting or foreground loading, allow cancellation
    if (state_ == State::CONNECTING || state_ == State::LOADING_CONTENT) {
        if (command == RemoteCommand::BACK) {
            dismissScreen();
            return;
        }
        // Ignore other commands while loading
        return;
    }

    // For both display content and background loading states
    if (state_ == State::DISPLAY_CONTENT || state_ == State::LOADING_BACKGROUND) {
        // Handle navigation commands specially for Kindle content
        switch (command) {
        case RemoteCommand::RIGHT:
            // Just call navigateNext which handles all the buffer checking internally
            navigateNext();
            break;
        case RemoteCommand::LEFT:
            navigatePrevious();
            break;
        case RemoteCommand::BACK:
        case RemoteCommand::UP:
            showPopupMenu();
            break;
        case RemoteCommand::ENTER:
            // Use this for page turn same as right
            navigateNext();
            break;
        case RemoteCommand::DOWN:
            // Show the popup menu here too for consistency with ReadableScreen
            showPopupMenu();
            break;
        case RemoteCommand::ANIMATION:
        default:
            break;
        }
    }
}

void KindleReadableScreen::setState(State newState) {
    // log_d("KindleReadableScreen state change: %d -> %d", static_cast<int>(state_),
    //       static_cast<int>(newState));
    state_ = newState;

    if (newState == State::CONNECTING) {
        // Start connection process
        // Launch the connection in the background
        cancelFlag_ = std::make_shared<CancelFlag>();

        DispatchGetCore0BackgroundQueue()->dispatch([this]() {
            std::string errorMessage;
            bool connected = connectToNetwork(false, errorMessage);

            if (!connected) {
                DispatchGetUiQueue()->dispatch(
                    [this, errorMessage]() { setErrorMessage(errorMessage); });
                return;
            }

            // Successfully connected, now load the book content
            DispatchGetUiQueue()->dispatch([this]() {
                if (dismissed_) {
                    return;
                }

                log_d("Setting loading message to \"Opening your Kindle book...\"");
                setLoadingMessage("Opening your Kindle book...");
                setState(State::LOADING_CONTENT);

                // Schedule a delayed message after 10 seconds if loading takes too long
                scheduleDelayedLoadingMessage();

                // Now load the content in the background queue
                DispatchGetCore0BackgroundQueue()->dispatch([this]() { loadKindleBookPage(); });
            });
        });
    }
}

void KindleReadableScreen::setLoadingMessage(const std::string &message, bool showRoundedRect) {
    if (dismissed_) {
        return;
    }

    loadingMessageView_.setText(message);
    showLoadingWithRoundedRect_ = showRoundedRect;
    loadingMessageView_.alignCenter = true; // Always ensure text is centered
    setNeedsRerender();
    display_.clearDisplay();
    render(display_);
}

auto KindleReadableScreen::connectToNetwork(bool useBleOnly, std::string &errorMessage) -> bool {
    bool wifiConnected = false;
    bool bleConnected = false;

    SindarinAPI::getInstance()->setAccessToken(prefs_.cloudToken.get().value());

#if CONFIG_USE_BLE_SERVER_PROXY
    // Try BLE first if not disabled
    if (!prefs_.bleDisabled.get().value_or(false)) {
        log_i("Attempting BLE connection for Kindle book...");

        // Disconnect WiFi if we're using BLE
        if (WiFi::getInstance().connected()) {
            log_i("Disconnecting WiFi since BLE is connecting");
            WiFi::getInstance().disconnectWiFi(3000);
        }

        if (!BLEManager::getInstance()->connectProxy(cancelFlag_)) {
            log_w("BLE connection failed for Kindle book");
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "kindle-ble-failed");
        } else {
            log_i("BLE successfully connected for Kindle book");
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "kindle-ble-connected");
            bleConnected = true;
            useBle_ = true;
        }
    }
#endif

    // Try WiFi if BLE failed and not in BLE-only mode
    if (!bleConnected && !useBleOnly && !prefs_.wifiDisabled.get().value_or(false)) {
        log_i("Attempting WiFi connection for Kindle book...");

        auto connected = WiFi::getInstance().connectWiFiFromSettings();
        if (connected != ESP_OK && connected != true) {
            log_w("WiFi connection failed for Kindle book");
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "kindle-wifi-failed");
            wifiConnected = false;
        } else {
            log_i("WiFi connection successful for Kindle book");
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "kindle-wifi-connected");
            wifiConnected = true;
        }
    } else if (useBleOnly) {
        log_i("BLE-only mode for Kindle book, skipping WiFi connection");
    } else if (!bleConnected) {
        log_i("WiFi is disabled in preferences, skipping WiFi connection");
    }

    if (!wifiConnected && !bleConnected) {
        log_w("Failed to establish any network connection for Kindle book");
        if (prefs_.wifiDisabled.get().value_or(false) && prefs_.bleDisabled.get().value_or(false)) {
            errorMessage = "Both WiFi and Bluetooth are disabled. Please enable one of them to "
                           "use Kindle books.";
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("kindle-books")},
                                      {"action", std::string("kindle-connection-failed")},
                                      {"reason", std::string("all-disabled")}});
        } else if (prefs_.wifiDisabled.get().value_or(false)) {
            errorMessage = "WiFi is disabled in settings and BLE connection failed. Please check "
                           "your BLE connection or enable WiFi to use Kindle books.";
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("kindle-books")},
                                      {"action", std::string("kindle-connection-failed")},
                                      {"reason", std::string("wifi-disabled-ble-failed")}});
        } else if (prefs_.bleDisabled.get().value_or(false)) {
            errorMessage = "Bluetooth is disabled in settings and WiFi connection failed. Please "
                           "check your WiFi connection or enable Bluetooth to use Kindle books.";
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("kindle-books")},
                                      {"action", std::string("kindle-connection-failed")},
                                      {"reason", std::string("ble-disabled-wifi-failed")}});
        } else {
            errorMessage = "Could not connect to network. Please check your WiFi and Bluetooth "
                           "settings and try again.";
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("kindle-books")},
                                      {"action", std::string("kindle-connection-failed")},
                                      {"reason", std::string("connection-failed")}});
        }
        return false;
    }

    return true;
}

void KindleReadableScreen::loadKindleBookPage() {
    if (dismissed_) {
        return;
    }

    // Get the SindarinAPI instance
    auto sindarinApi = SindarinAPI::getInstance();
    if (!sindarinApi) {
        log_e("Failed to get SindarinAPI instance");
        DispatchGetUiQueue()->dispatch(
            [this]() { setErrorMessage("Internal error. Please try again later."); });
        return;
    }

    // Create a response object
    auto response = std::make_shared<KindlePageResponse>();
    if (!response) {
        log_e("Failed to create KindlePageResponse object");
        DispatchGetUiQueue()->dispatch(
            [this]() { setErrorMessage("Internal error. Please try again later."); });
        return;
    }

    // For initial load, always open the book with its title - this API call runs in background
    // thread First check if the title contains '+' characters which might indicate it's already URL
    // encoded
    std::string cleanTitle = kindleBook_->title;
    if (cleanTitle.find('+') != std::string::npos) {
        log_i("Title appears to be URL encoded, contains '+' characters: %s", cleanTitle.c_str());
        // Replace '+' with spaces to prevent double encoding
        std::string::size_type pos = 0;
        while ((pos = cleanTitle.find('+', pos)) != std::string::npos) {
            cleanTitle.replace(pos, 1, " ");
            pos++;
        }
        log_i("Cleaned title for API call: %s", cleanTitle.c_str());
    }

    // Ensure we have a valid title to prevent crashes
    if (cleanTitle.empty()) {
        log_e("Book title is empty, cannot load Kindle book");
        DispatchGetUiQueue()->dispatch(
            [this]() { setErrorMessage("Book title is missing. Please try another book."); });
        return;
    }

    log_i("Calling openKindleBook API with title: %s (%s cancel flag)", cleanTitle.c_str(),
          cancelFlag_ ? "valid" : "invalid");

    // Call the API (no try-catch as exceptions are disabled)
    int httpCode = sindarinApi->openKindleBook(cleanTitle, response.get(), cancelFlag_, useBle_);
    log_i("openKindleBook returned HTTP code: %d", httpCode);

    if (httpCode != 200 || (response && !response->success)) {
        // Handle error on the UI thread with specific error message based on HTTP code
        DispatchGetUiQueue()->dispatch([this, httpCode]() {
            std::string errorMessage;

            // Customize error message based on HTTP code
            if (httpCode == 500) {
                errorMessage = "Server error. Please try again later.";
            } else if (httpCode == 401 || httpCode == 403) {
                errorMessage = "Authentication error. Please try signing in again.";
            } else if (httpCode == 404) {
                errorMessage = "Book not found in your Kindle library.";
            } else if (httpCode >= 400 && httpCode < 500) {
                errorMessage = "Request error. Try syncing your Kindle library.";
            } else if (httpCode >= 500) {
                errorMessage = "Server error. Please try again later.";
            } else {
                errorMessage = "Failed to load book content.";
            }

            log_i("HTTP error %d when loading book content", httpCode);

            // Cancel the delayed message since we're showing an error
            cancelDelayedLoadingMessage();

            setErrorMessage(errorMessage);
        });
        return;
    }

    // Verify response data to avoid crashes
    if (!response || response->ocrText.empty()) {
        log_e("Empty or invalid response data from Kindle API");
        DispatchGetUiQueue()->dispatch([this]() {
            cancelDelayedLoadingMessage();
            setErrorMessage("Received empty content from Kindle. Please try again later.");
        });
        return;
    }

    // Process the response on the UI thread
    DispatchGetUiQueue()->dispatch([this, response]() {
        if (dismissed_) {
            return;
        }

        log_i("Processing Kindle book page response (success=%d, text length=%zu)",
              response->success, response->ocrText.length());

        // Cancel the delayed message since content loaded successfully
        cancelDelayedLoadingMessage();

        // Update current page information
        currentPage_ = response->progress.currentPage;
        totalPages_ = response->progress.totalPages;
        currentPageText_ = response->ocrText;
        currentPageImageUrl_ = response->screenshotUrl;

        // Update the book progress in the KindleBook object
        kindleBook_->progressPercent = response->progress.percentage;
        kindleBook_->progress = StringFormat("%d%%", response->progress.percentage);

        // Initial open book call is like a navigate
        lastCallWasPreview_ = false;
        log_i("API RESPONSE - Initial page: %d/%d (%d%%), new text size: %zu bytes", currentPage_,
              totalPages_, response->progress.percentage, currentPageText_.length());

        // Create a stream with the text content
        auto stream = std::make_shared<StdStringStream>(currentPageText_, DocType::TXT);
        renderer_.setStream(stream);

        // Create a place for the content
        auto place = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));

        // Clear the screen and prepare for content
        clearScreenForContent();

        // Render the page
        uint8_t darkPicturePercent = 0;

        // Set initial place
        newPlace_ = place;

        // Initialize current place
        currentPlace_ = place;

        // Render with the place
        std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(currentPlace_);

        // Calculate initial page size
        if (newPlace_ && currentPlace_) {
            uint32_t newOffset = newPlace_->getOffset();
            uint32_t currOffset = currentPlace_->getOffset();
            lastRenderedPageSize_ = (newOffset > currOffset) ? (newOffset - currOffset) : 0;
            log_i("Initial page size: %" PRIu32 " bytes", lastRenderedPageSize_);
        }

        log_i("Kindle page rendered, forcing display update");

        // No need to call turnPage() here since we've already set currentPlace_

        // Explicitly force a full display update
        display_.updateDisplay(DisplaySystem::UpdateType::FULL, true);

        // Now change state after we've rendered everything
        setState(State::DISPLAY_CONTENT);

        // Update flags for future renders
        firstKindleLoad_ = false;
        hasNavigatedForward_ = false; // Reset navigation tracking for fresh book load

        // Reset any preview-related flags on initial load
        previewRequestInProgress_ = false;
        needsNavigateSync_ = false;
        lastPreviewOffset_ = 0;
        justNavigatedToServer_ = false;
        // For initial load, pretend we just did a navigate call so that preview is the next call
        lastCallWasPreview_ = false;

        // Log clear debug info about the initial state
        uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
        uint32_t totalSize = renderer_.size();
        log_i("[INITIAL LOAD] Current position: %" PRIu32 ", buffer size: %" PRIu32
              ", preview boundary: %" PRIu32 ", lastCallWasPreview: %d",
              currentOffset, totalSize, lastPreviewOffset_, lastCallWasPreview_);

        // Load just one page of content after initial load
        // We don't need to check buffer size for the very first preview
        log_i("[INITIAL LOAD] Loading first preview after initial load");

        // Add a small delay to ensure UI is fully updated before loading more content
        // This helps with stability and prevents race conditions
        DispatchGetUiQueue()->dispatch([this]() {
            if (!dismissed_) {
                // Small delay to ensure initial rendering is complete
                vTaskDelay(pdMS_TO_TICKS(100));

                // Double-check conditions haven't changed during delay
                if (!dismissed_ && !previewRequestInProgress_ && !lastCallWasPreview_) {
                    log_i("[INITIAL LOAD] Starting first preview request after delay");
                    // This will correctly set lastCallWasPreview_ = true when complete
                    loadNextKindlePreview();
                }
            }
        });
    });
}

void KindleReadableScreen::setErrorMessage(const std::string &errorMessage) {
    if (dismissed_) {
        return;
    }

    // Clear any previous auto-dismiss flag/task
    if (autoDismissCancelFlag_) {
        CancelFlag::cancel(autoDismissCancelFlag_);
        autoDismissCancelFlag_ = nullptr;
    }

    loadingMessageView_.setHidden(true);
    errorMessageView_.setText(errorMessage);
    errorMessageView_.setHidden(false);
    errorMessageView_.draw();

    log_i("Showing error message: %s", errorMessage.c_str());

    // Update state and trigger a redraw
    setState(State::ERROR);
    setNeedsLayout();
    setNeedsRerender();

    // Clear the display to remove any previous content or loading messages
    display_.clearDisplay();

    draw();

    display_.updateDisplay(DisplaySystem::UpdateType::FULL, true);

    // Create a new cancel flag for this auto-dismiss task
    autoDismissCancelFlag_ = std::make_shared<CancelFlag>();
    std::shared_ptr<CancelFlag> localFlag = autoDismissCancelFlag_; // Capture for lambda

    // Automatically dismiss after 5 seconds
    DispatchGetCore0BackgroundQueue()->dispatch([this, localFlag]() {
        // Wait for 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));

        // Check if the task was cancelled before dismissing
        if (!localFlag || CancelFlag::isCancelled(localFlag)) {
            log_i("Auto-dismiss task was cancelled");
            return;
        }

        // Then dismiss on the UI thread
        DispatchGetUiQueue()->dispatch([this]() {
            if (!dismissed_) {
                log_i("Auto-dismissing error screen after delay");
                dismissScreen();
            } else {
                log_i("Screen already dismissed, not auto-dismissing");
            }
        });
    });
}

void KindleReadableScreen::loadNextKindlePreview() {
    if (dismissed_ || nextPageLoading_ || previewRequestInProgress_) {
        // Don't initiate a preview if we're already loading something or if a preview is in
        // progress
        return;
    }

    // CRITICAL: Enforce strict alternation between preview and navigate
    if (lastCallWasPreview_) {
        // Check if we've crossed the preview boundary - in that case, we MUST sync
        // This is a critical exception to the alternation pattern
        uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
        bool hasCrossedBoundary = (currentOffset >= lastPreviewOffset_ && lastPreviewOffset_ > 0);

        if (hasCrossedBoundary) {
            log_i("CRITICAL OVERRIDE: Preview boundary crossed (position %" PRIu32
                  " >= boundary %" PRIu32
                  ") and lastCallWasPreview=true - MUST sync now despite alternation pattern",
                  currentOffset, lastPreviewOffset_);

            // This is a special case where we MUST break alternation to maintain server sync
            syncWithServer();
            return;
        }

        // Normal case where we haven't crossed a boundary
        log_i("Preview blocked: Last call was already a preview");
        // Mark that we need a sync before next preview, but don't force it now
        // This prevents navigate-navigate sequence
        needsNavigateSync_ = true;
        return;
    }

    // Set flags to prevent multiple concurrent load requests
    nextPageLoading_ = true;
    previewRequestInProgress_ = true;

    // Mark the state for internal tracking
    setState(State::LOADING_BACKGROUND);
    log_i("Starting preview request");

    // Execute the API call on the background thread
    DispatchGetCore0BackgroundQueue()->dispatch([this]() {
        // Create a response object
        auto response = std::make_shared<KindlePageResponse>();

        // Make the request
        auto sindarinApi = SindarinAPI::getInstance();
        int httpCode = sindarinApi->previewKindleNext(response.get(), cancelFlag_, useBle_);

        if (httpCode != 200 || !response->success) {
            DispatchGetUiQueue()->dispatch([this, httpCode]() {
                if (dismissed_) {
                    return;
                }

                nextPageLoading_ = false;
                previewRequestInProgress_ = false;

                // For preview loads, just log the error and restore state
                log_w("Preview loading failed with HTTP code %d", httpCode);
                setState(State::DISPLAY_CONTENT);
            });
            return;
        }

        // Process the response on the UI thread
        DispatchGetUiQueue()->dispatch([this, response]() {
            if (dismissed_) {
                return;
            }

            nextPageLoading_ = false;

            log_i("Processing preview response (success=%d, length=%zu)", response->success,
                  response->ocrText.length());

            // NEVER set a new preview boundary if one already exists and hasn't been processed
            if (lastPreviewOffset_ == 0) {
                // Store the current buffer size before adding new content
                uint32_t previousSize = currentPageText_.length();
                uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;

                // CRITICAL: Calculate the boundary where we should navigate
                // Set the boundary AT THE EXACT START of the new content
                // This ensures we navigate as soon as we reach the first character of the preview
                // content
                uint32_t boundary = previousSize > 0 ? previousSize : 0;

                // Store the boundary - this marks where navigate should happen exactly
                lastPreviewOffset_ = boundary;
                log_i("PREVIEW BOUNDARY: Setting new preview boundary at %" PRIu32
                      " (current position: %" PRIu32 ", buffer size: %" PRIu32 ")",
                      lastPreviewOffset_, currentOffset, previousSize);

                // If the current position is already at or beyond the preview boundary,
                // we should immediately mark for sync on the next navigation
                if (currentOffset >= lastPreviewOffset_ && lastPreviewOffset_ > 0) {
                    log_i("PREVIEW BOUNDARY: Current position (%" PRIu32
                          ") is already at or beyond preview boundary (%" PRIu32
                          "), marking for sync",
                          currentOffset, lastPreviewOffset_);
                    needsNavigateSync_ = true;
                }
            } else {
                uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
                log_i("PREVIEW BOUNDARY: Keeping existing preview boundary at %" PRIu32
                      " (current position: %" PRIu32 ")",
                      lastPreviewOffset_, currentOffset);
            }

            // Append the new text to the existing buffer
            if (!currentPageText_.empty()) {
                // Check if the next page starts with a non-lowercase character to decide on spacing
                const std::string &nextText = response->ocrText;
                if (!nextText.empty() && !std::islower(static_cast<unsigned char>(nextText[0]))) {
                    // If starts with non-lowercase character (uppercase, punctuation, digits,
                    // etc.), add paragraph break
                    currentPageText_ += "\n\n";
                } else {
                    // If it's a lowercase character, just add a space for continuous reading
                    currentPageText_ += " ";
                }
            }
            currentPageText_ += response->ocrText;

            log_i("Added %zu bytes to buffer, new size: %zu bytes", response->ocrText.length(),
                  currentPageText_.length());

            // Save the current position before updating the stream
            uint32_t savedOffset = currentPlace_ ? currentPlace_->getOffset() : 0;

            // Create a stream with the text content
            auto stream = std::make_shared<StdStringStream>(currentPageText_, DocType::TXT);
            renderer_.setStream(stream);

            // Restore the place after setting the stream - this is critical to prevent
            // jumping back to the beginning when new content is loaded
            if (currentPlace_ && savedOffset > 0) {
                // Create a place at the same offset as before
                auto restoredPlace =
                    std::dynamic_pointer_cast<TxtPlace>(Place::factory(savedOffset));
                currentPlace_ = restoredPlace;
                renderer_.setCurrentPlace(currentPlace_);
                log_i("Restored reading position to offset %" PRIu32, savedOffset);
            }

            // Update state
            setState(State::DISPLAY_CONTENT);
            previewRequestInProgress_ = false;

            // CRITICAL: Mark that the last API call was a preview
            lastCallWasPreview_ = true;

            // Check if we should proactively load more content for short pages
            uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
            uint32_t totalSize = renderer_.size();
            uint32_t remainingBytes = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;

            // For very short pages, load one more page only if we're below threshold
            if (remainingBytes < bufferThreshold_ && response->ocrText.length() < 500) {
                log_i("Short page detected, preloading additional content");
                // Will load just one more page, respecting alternation
                loadNextContent();
            }

            // Check if boundary handling is needed based on current position
            if (savedOffset < lastPreviewOffset_) {
                log_i("Current position (%" PRIu32 ") is before preview boundary (%" PRIu32 ")",
                      savedOffset, lastPreviewOffset_);
            }

            // Calculate buffer information
            // Define previousSize here to avoid redefinition issues
            uint32_t previousSize =
                totalSize - response->ocrText.length(); // Calculate size before adding new content

            // Calculate remaining bytes before and after new content was loaded
            uint32_t remainingBytesBefore =
                (currentOffset < previousSize) ? (previousSize - currentOffset) : 0;
            uint32_t remainingBytesNow =
                (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;

            // Flag if we were near the end of the buffer before new content was loaded
            bool wasNearEndOfBuffer = remainingBytesBefore < bufferThreshold_;
            bool shouldRefreshPage = false;

            // Debug logging for buffer status
            if (needsRedrawAfterLoad_) {
                log_i("New content loaded, page needs refresh");
            }

            // We should refresh the page if we were near the end of the buffer before new content
            // was loaded or explicitly marked for redraw, and we now have more content available
            if ((wasNearEndOfBuffer || needsRedrawAfterLoad_) &&
                remainingBytesNow > remainingBytesBefore) {
                log_i("Refreshing page with new content");

                // Re-render the current page with the new content
                uint8_t darkPicturePercent = 0;
                std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(currentPlace_);

                // Update the display
                display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                                       display_.darkPictureVComAdj(darkPicturePercent));

                // Calculate new page size
                if (newPlace_ && currentPlace_) {
                    uint32_t newOffset = newPlace_->getOffset();
                    uint32_t currOffset = currentPlace_->getOffset();
                    lastRenderedPageSize_ = (newOffset > currOffset) ? (newOffset - currOffset) : 0;
                    log_i("New page size after refresh: %" PRIu32 " bytes", lastRenderedPageSize_);
                }

                shouldRefreshPage = true;
                needsRedrawAfterLoad_ = false;
            }

            // Check if we're still low on content and need to load more
            if (!shouldRefreshPage && checkNearEndOfBuffer()) {
                log_i("Still need more content, loading next page");
                loadNextKindlePreview();
            }
        });
    });
}

void KindleReadableScreen::loadNextKindlePage() {
    if (dismissed_ || nextPageLoading_) {
        return;
    }

    // Set flag to prevent multiple concurrent load requests
    nextPageLoading_ = true;

    // Determine if we're loading in foreground or background
    bool isBackgroundLoading = (state_ == State::DISPLAY_CONTENT);

    if (!isBackgroundLoading) {
        // Only show loading UI if this is not background loading
        setState(State::LOADING_CONTENT);
        setLoadingMessage("Loading next page...");
        setNeedsRerender();
    } else {
        // Just mark the state for internal tracking
        setState(State::LOADING_BACKGROUND);
        log_i("Starting background load of next Kindle page");
    }

    // Execute the API call on the background thread
    DispatchGetCore0BackgroundQueue()->dispatch([this, isBackgroundLoading]() {
        // Create a response object
        auto response = std::make_shared<KindlePageResponse>();

        // Make the request - for explicit navigation, we always use ocr=0
        // We should have already pre-cached content with preview-next
        auto sindarinApi = SindarinAPI::getInstance();
        int httpCode = sindarinApi->navigateKindleNext(response.get(), cancelFlag_, useBle_, false);

        if (httpCode != 200 || !response->success) {
            DispatchGetUiQueue()->dispatch([this, isBackgroundLoading, httpCode]() {
                if (dismissed_) {
                    return;
                }

                nextPageLoading_ = false;

                if (isBackgroundLoading) {
                    // For background loads, just log the error and restore state
                    log_w("Background loading of next Kindle page failed with HTTP code %d",
                          httpCode);
                    setState(State::DISPLAY_CONTENT);
                } else {
                    // For foreground loads, show the error
                    // Customize error message based on HTTP code
                    std::string errorMessage;

                    if (httpCode == 500) {
                        errorMessage = "Server error. Please try again later.";
                    } else if (httpCode == 401 || httpCode == 403) {
                        errorMessage = "Authentication error. Please try signing in again.";
                    } else if (httpCode == 404) {
                        errorMessage = "Page not found in your Kindle book.";
                    } else if (httpCode >= 400 && httpCode < 500) {
                        errorMessage = "Request error. Please check your settings.";
                    } else if (httpCode >= 500) {
                        errorMessage = "Server error. Please try again later.";
                    } else {
                        errorMessage = "Failed to load next page.";
                    }

                    log_i("HTTP error %d when loading next page", httpCode);
                    setErrorMessage(errorMessage);
                }
            });
            return;
        }

        // Process the response on the UI thread
        DispatchGetUiQueue()->dispatch([this, response, isBackgroundLoading]() {
            if (dismissed_) {
                return;
            }

            nextPageLoading_ = false;

            log_i("Processing Kindle navigation next response (success=%d, ocr=false)",
                  response->success);

            // Mark that we've successfully made a remote forward navigation call
            hasNavigatedForward_ = true;

            // Update current page information
            currentPage_ = response->progress.currentPage;
            totalPages_ = response->progress.totalPages;
            currentPageImageUrl_ = response->screenshotUrl;

            // Update the book progress in the KindleBook object
            kindleBook_->progressPercent = response->progress.percentage;
            kindleBook_->progress = StringFormat("%d%%", response->progress.percentage);

            log_i("Updated book progress metadata: page %d/%d (%d%%)", currentPage_, totalPages_,
                  response->progress.percentage);

            // Note: we don't append text because we're using ocr=0 since
            // we should already have the content from the preview call
            log_i("Skipping text append since OCR was not requested (using previewed content)");

            log_i("API RESPONSE - Next page: %d/%d (%d%%), new text size: %zu bytes, total buffer "
                  "size now: %zu bytes",
                  currentPage_, totalPages_, response->progress.percentage,
                  response->ocrText.length(), currentPageText_.length());

            // Save the current position before updating the stream
            uint32_t savedOffset = currentPlace_ ? currentPlace_->getOffset() : 0;

            // Create a stream with the text content
            auto stream = std::make_shared<StdStringStream>(currentPageText_, DocType::TXT);
            renderer_.setStream(stream);

            // Restore the place after setting the stream - this is critical to prevent
            // jumping back to the beginning when new content is loaded
            if (currentPlace_ && savedOffset > 0) {
                // Create a place at the same offset as before
                auto restoredPlace =
                    std::dynamic_pointer_cast<TxtPlace>(Place::factory(savedOffset));
                currentPlace_ = restoredPlace;
                renderer_.setCurrentPlace(currentPlace_);
                log_i("Restored place to offset %" PRIu32 " after loading new content",
                      savedOffset);
            }

            if (isBackgroundLoading) {
                // For background loading, update the stream and restore state
                log_i("Background loading complete, updated text buffer");
                setState(State::DISPLAY_CONTENT);

                // Get current offset and check if the current page is incomplete
                uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;

                // Calculate how much content we had before the new load and how much we have now
                uint32_t totalSize = renderer_.size();
                uint32_t loadedSize = response->ocrText.length();
                uint32_t previousSize = totalSize - loadedSize;

                // Calculate remaining bytes before and after new content was loaded
                uint32_t remainingBytesBefore =
                    (currentOffset < previousSize) ? (previousSize - currentOffset) : 0;
                uint32_t remainingBytesNow =
                    (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;

                // Flag if we were near the end of the buffer before new content was loaded
                bool wasNearEndOfBuffer = remainingBytesBefore < bufferThreshold_;
                bool shouldRefreshPage = false;

                // Log detailed buffer information for debugging
                log_i("Buffer status - Before load: offset=%" PRIu32 "/%" PRIu32
                      " (remaining: %" PRIu32 " bytes), After load: size=%" PRIu32
                      " (remaining: %" PRIu32 " bytes), Added: %" PRIu32 " bytes, needsRedraw: %s",
                      currentOffset, previousSize, remainingBytesBefore, totalSize,
                      remainingBytesNow, loadedSize, needsRedrawAfterLoad_ ? "yes" : "no");

                // We should refresh the page if:
                // 1. We were near the end of the buffer before new content was loaded (or marked
                // for redraw)
                // 2. We now have more content available in the buffer
                if ((wasNearEndOfBuffer || needsRedrawAfterLoad_) &&
                    remainingBytesNow > remainingBytesBefore) {
                    log_i("Refreshing page after background load - %s "
                          "(before: remaining=%" PRIu32 " bytes, now: remaining=%" PRIu32 " bytes)",
                          needsRedrawAfterLoad_ ? "explicitly marked for redraw"
                                                : "near the end of buffer",
                          remainingBytesBefore, remainingBytesNow);

                    // Re-render the current page with the new content
                    uint8_t darkPicturePercent = 0;
                    std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(currentPlace_);

                    // Update the display
                    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                                           display_.darkPictureVComAdj(darkPicturePercent));

                    // Calculate new page size
                    if (newPlace_ && currentPlace_) {
                        uint32_t newOffset = newPlace_->getOffset();
                        uint32_t currOffset = currentPlace_->getOffset();
                        lastRenderedPageSize_ =
                            (newOffset > currOffset) ? (newOffset - currOffset) : 0;
                        log_i("New page size after refresh: %" PRIu32 " bytes",
                              lastRenderedPageSize_);
                    }

                    shouldRefreshPage = true;

                    // Reset the redraw flag since we've now handled it
                    needsRedrawAfterLoad_ = false;
                }

                // Check if we're still low on content and need to load more
                if (!shouldRefreshPage && checkNearEndOfBuffer()) {
                    log_i("Still near buffer end after background load, loading more preview "
                          "content");
                    loadNextKindlePreview();
                }
            } else {
                // For foreground loading, render the page immediately
                // Create a place for the content
                auto place = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));

                // Clear the screen and prepare for content
                clearScreenForContent();

                // Render the page
                uint8_t darkPicturePercent = 0;
                std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(place);

                log_i("Kindle next page rendered, forcing display update");

                // Explicitly force a full display update
                display_.updateDisplay(DisplaySystem::UpdateType::FULL, true);

                // Save the place
                turnPage();

                // Now change state after we've rendered everything
                setState(State::DISPLAY_CONTENT);

                // Proactively check if we need to start loading the next page
                if (checkNearEndOfBuffer()) {
                    log_i("Near buffer end after load, proactively loading preview content");
                    loadNextKindlePreview();
                }
            }
        });
    });
}

void KindleReadableScreen::navigateNext() {
    // Allow navigation from DISPLAY_CONTENT or LOADING_BACKGROUND states
    if ((state_ != State::DISPLAY_CONTENT && state_ != State::LOADING_BACKGROUND) || dismissed_) {
        return;
    }

    // --- Local Navigation First ---

    // Get current buffer info before the turn
    uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
    uint32_t totalSize = renderer_.size();
    uint32_t remainingBytes = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;
    uint32_t pageSize = lastRenderedPageSize_ > 0 ? lastRenderedPageSize_ : 400;

    // Check if we can turn the page locally
    bool canTurnPageLocally =
        newPlace_ && currentPlace_ && newPlace_->getOffset() > currentPlace_->getOffset();

    // If we cannot turn the page locally, we might need to load content
    if (!canTurnPageLocally) {
        log_i("NAVIGATE: Cannot turn page locally (current: %" PRIu32 ", next: %s)", currentOffset,
              newPlace_ ? std::to_string(newPlace_->getOffset()).c_str() : "null");

        // If we are near the end, try loading more content
        if (remainingBytes < bufferThreshold_) {
            log_i("NAVIGATE: At end of local buffer (remaining: %" PRIu32
                  "), attempting to load more.",
                  remainingBytes);
            if (!nextPageLoading_ && !previewRequestInProgress_) {
                loadNextContent(); // Handles preview/navigate alternation
            } else {
                log_i("NAVIGATE: Load already in progress, showing loading message.");
                setLoadingMessage("Loading more...", true); // Show loading indicator
                if (state_ != State::LOADING_BACKGROUND) {
                    setState(State::LOADING_BACKGROUND);
                }
                setNeedsRerender();
            }
        } else {
            log_w("NAVIGATE: Cannot turn page locally, but not near end of buffer? (remaining: "
                  "%" PRIu32 ")",
                  remainingBytes);
            // This state shouldn't ideally happen unless buffer is full but renderPage failed?
        }
        return; // Stop processing navigateNext if we can't turn locally
    }

    // --- Perform Local Page Turn ---
    uint32_t offsetBeforeTurn = currentPlace_ ? currentPlace_->getOffset() : 0;
    turnPage(); // This updates currentPlace_ and lastRenderedPageSize_
    uint32_t offsetAfterTurn = currentPlace_ ? currentPlace_->getOffset() : 0;

    // Render the new page content locally
    uint8_t darkPicturePercent = 0;
    std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(currentPlace_);
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                           display_.darkPictureVComAdj(darkPicturePercent));
    hasNavigatedForward_ = true; // Mark that user has moved forward

    // --- Debug Logging: Show context around page turn ---
    logPageContext(offsetBeforeTurn, offsetAfterTurn);
    // --- End Debug Logging ---

    log_i("[NAVIGATE LOCAL] Turned page: %" PRIu32 " -> %" PRIu32 ", Preview Boundary: %" PRIu32
          ", LastCallPreview: %d",
          offsetBeforeTurn, offsetAfterTurn, lastPreviewOffset_, lastCallWasPreview_);

    // --- Boundary Detection & Sync Decision --- PREDICTIVE CHECK FOR *NEXT* TURN

    // Predict if the *next* page turn (after the one we just did) will cross the boundary
    uint32_t nextPotentialOffset =
        newPlace_ ? newPlace_->getOffset() : offsetAfterTurn; // Where the *next* turn would land
    uint32_t nextPotentialPageEnd = nextPotentialOffset + pageSize; // Estimate end of *next* page

    bool nextTurnWillCrossBoundary =
        (lastPreviewOffset_ > 0 &&
         (nextPotentialOffset >= lastPreviewOffset_ ||
          nextPotentialPageEnd >= lastPreviewOffset_) &&
         offsetAfterTurn < lastPreviewOffset_); // Ensure we haven't already crossed

    log_i("[NAVIGATE PREDICT] Next Turn Prediction - current: %" PRIu32 ", next: %" PRIu32
          ", next+size: %" PRIu32 ", boundary: %" PRIu32 ", nextWillCross: %d",
          offsetAfterTurn, nextPotentialOffset, nextPotentialPageEnd, lastPreviewOffset_,
          nextTurnWillCrossBoundary);

    // --- Sync Logic ---

    // Condition 1: Have we *already* crossed the boundary with the turn we just made?
    bool justCrossedBoundary = (lastPreviewOffset_ > 0 && offsetAfterTurn >= lastPreviewOffset_ &&
                                offsetBeforeTurn < lastPreviewOffset_);

    if (justCrossedBoundary) {
        log_i("[SYNC DECISION] Just crossed boundary (Offset %" PRIu32 " >= %" PRIu32
              "). Marking for sync.",
              offsetAfterTurn, lastPreviewOffset_);
        needsNavigateSync_ = true;
    }

    // Now, decide whether to sync immediately or defer
    bool shouldSyncNow = false;

    // A. Normal Sync: If marked for sync and the last call wasn't preview.
    if (needsNavigateSync_ && !lastCallWasPreview_) {
        log_i("[SYNC ACTION] Condition Met: needsNavigateSync_ is true AND lastCallWasPreview_ is "
              "false. Syncing now.");
        shouldSyncNow = true;
    }
    // B. Override Sync 1: If we JUST crossed the boundary during this turn, sync now even if last
    // call was preview.
    else if (justCrossedBoundary && lastCallWasPreview_) {
        log_i("[SYNC ACTION] Override Condition Met (Post-Crossing): Just crossed boundary AND "
              "lastCallWasPreview_ is true. Syncing now (breaking alternation).");
        shouldSyncNow = true;
    }
    // C. Override Sync 2: If the *next* turn is predicted to cross the boundary AND the last call
    // *was* preview.
    else if (nextTurnWillCrossBoundary && lastCallWasPreview_) {
        log_i("[SYNC ACTION] Override Condition Met (Predictive): Next turn will cross boundary "
              "AND lastCallWasPreview_ is true. Syncing now (breaking alternation).");
        shouldSyncNow = true;
        // Ensure sync is still marked needed in case sync fails or boundary persists
        // syncWithServer will handle resetting lastCallWasPreview_ and potentially
        // lastPreviewOffset_.
        needsNavigateSync_ = true;
    }

    if (shouldSyncNow && !nextPageLoading_) {
        syncWithServer(); // Calls /navigate-next, resets lastCallWasPreview_, clears
                          // lastPreviewOffset_
        needsNavigateSync_ =
            false; // Sync initiated, clear the flag (syncWithServer might reset boundary)
    } else if (needsNavigateSync_) {
        log_i("[SYNC DEFERRED] Marked for sync (needsNavigateSync_=true) but conditions not met "
              "for immediate sync (lastCallWasPreview=%d, nextTurnWillCross=%d). Will sync later.",
              lastCallWasPreview_, nextTurnWillCrossBoundary);
    }

    // --- Background Loading Check ---

    // Check if we need to load more content in the background after the turn
    uint32_t remainingBytesAfterTurn =
        (offsetAfterTurn < totalSize) ? (totalSize - offsetAfterTurn) : 0;
    if (remainingBytesAfterTurn < bufferThreshold_ && !nextPageLoading_ &&
        !previewRequestInProgress_ && !lastCallWasPreview_) {
        log_i("[NAVIGATE LOAD] Near end of buffer after turn (remaining: %" PRIu32
              "), loading preview in background.",
              remainingBytesAfterTurn);
        loadNextKindlePreview(); // Load more content if needed and allowed
    }
}

void KindleReadableScreen::loadPreviousKindlePage() {
    if (state_ != State::DISPLAY_CONTENT || dismissed_) {
        return;
    }

    setState(State::LOADING_CONTENT);
    setLoadingMessage("Loading previous page...");
    setNeedsRerender();

    // Execute the API call on the background thread
    DispatchGetCore0BackgroundQueue()->dispatch([this]() {
        // Create a response object
        auto response = std::make_shared<KindlePageResponse>();

        // Make the request
        auto sindarinApi = SindarinAPI::getInstance();
        int httpCode = sindarinApi->navigateKindlePrevious(response.get(), cancelFlag_, useBle_);

        if (httpCode != 200 || !response->success) {
            DispatchGetUiQueue()->dispatch([this, httpCode]() {
                std::string errorMessage;

                // Customize error message based on HTTP code
                if (httpCode == 500) {
                    errorMessage = "Server error. Please try again later.";
                } else if (httpCode == 401 || httpCode == 403) {
                    errorMessage = "Authentication error. Please try signing in again.";
                } else if (httpCode == 404) {
                    errorMessage = "Previous page not found in your Kindle book.";
                } else if (httpCode >= 400 && httpCode < 500) {
                    errorMessage = "Request error. Please check your settings.";
                } else if (httpCode >= 500) {
                    errorMessage = "Server error. Please try again later.";
                } else {
                    errorMessage = "Failed to load previous page.";
                }

                log_i("HTTP error %d when loading previous page", httpCode);
                setErrorMessage(errorMessage);
            });
            return;
        }

        // Process the response on the UI thread
        DispatchGetUiQueue()->dispatch([this, response]() {
            if (dismissed_) {
                return;
            }

            log_i("Processing Kindle navigation previous response (success=%d, text length=%zu)",
                  response->success, response->ocrText.length());

            // Update current page information
            currentPage_ = response->progress.currentPage;
            totalPages_ = response->progress.totalPages;

            // For previous page, we replace the current buffer
            currentPageText_ = response->ocrText;
            currentPageImageUrl_ = response->screenshotUrl;

            // Update the book progress in the KindleBook object
            kindleBook_->progressPercent = response->progress.percentage;
            kindleBook_->progress = StringFormat("%d%%", response->progress.percentage);

            log_i("API RESPONSE - Previous page: %d/%d (%d%%), new text size: %zu bytes",
                  currentPage_, totalPages_, response->progress.percentage,
                  currentPageText_.length());

            // Create a stream with the text content
            auto stream = std::make_shared<StdStringStream>(currentPageText_, DocType::TXT);
            renderer_.setStream(stream);

            // Create a place for the content
            auto place = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));

            // Clear the screen and prepare for content
            clearScreenForContent();

            // Render the page
            uint8_t darkPicturePercent = 0;
            std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(place);

            log_i("Kindle previous page rendered, forcing display update");

            // Explicitly force a full display update
            display_.updateDisplay(DisplaySystem::UpdateType::FULL, true);

            // Save the place
            turnPage();

            // Reset navigation tracking since we're loading a new page from the beginning
            hasNavigatedForward_ = false;

            // Set lastCallWasPreview_ = false since we just did a navigate-previous
            lastCallWasPreview_ = false;

            // Now change state after we've rendered everything
            setState(State::DISPLAY_CONTENT);
        });
    });
}

void KindleReadableScreen::navigatePrevious() {
    if (state_ != State::DISPLAY_CONTENT || dismissed_) {
        return;
    }

    // Get the current place offset
    uint32_t currentOffset = 0;
    if (currentPlace_) {
        currentOffset = currentPlace_->getOffset();
    }

    // If at the beginning, we need to fetch from the API
    if (currentOffset == 0) {
        // Only allow going to previous Kindle page if user hasn't navigated forward yet
        if (!hasNavigatedForward_) {
            loadPreviousKindlePage();
        } else {
            log_i("Preventing navigation to previous Kindle page after forward navigation");
        }
        return;
    }

    // Try to get the previous place from the renderer
    auto prevPlace = renderer_.getPreviousPlace();

    // If it's null, we need to compute a reasonable fallback
    if (!prevPlace) {
        // Calculate a reasonable page size - from logs we can see the rendered page size
        // If available, use the page size from the last navigation
        uint32_t lastPageSize = 0;
        if (newPlace_ && currentPlace_) {
            lastPageSize =
                currentPlace_->getOffset() -
                (newPlace_->getOffset() > currentPlace_->getOffset() ? 0 : newPlace_->getOffset());
        }

        // Use a default if we couldn't determine page size
        uint32_t pageSize = (lastPageSize > 0) ? lastPageSize : 500;

        // Ensure we don't go before start of buffer
        uint32_t prevOffset = (currentOffset > pageSize) ? (currentOffset - pageSize) : 0;

        log_w("No valid previous place found, using fallback navigation offset %" PRIu32
              " (pageSize: %" PRIu32 ")",
              prevOffset, pageSize);

        prevPlace = Place::factory(prevOffset);
    }

    // Get the offset for logging
    uint32_t prevOffset = prevPlace->getOffset();
    uint32_t totalSize = renderer_.size();
    uint32_t pageSize = currentOffset - prevOffset;

    log_i("NAVIGATION - Moving backward from offset %" PRIu32 " to %" PRIu32 " (page size: %" PRIu32
          " bytes), buffer size: %" PRIu32,
          currentOffset, prevOffset, pageSize, totalSize);

    // Set this as our new place
    newPlace_ = prevPlace;

    // Update current place and renderer state
    turnPage();

    // Render the content from the new position
    uint8_t darkPicturePercent = 0;
    std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(currentPlace_);

    // Update the display
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                           display_.darkPictureVComAdj(darkPicturePercent));

    log_i("Navigated to previous page within current buffer");
}

auto KindleReadableScreen::checkNearEndOfBuffer() -> bool {
    if (!renderer_.getStream()) {
        return true;
    }

    uint32_t currentOffset = 0;
    if (renderer_.getCurrentPlace()) {
        currentOffset = renderer_.getCurrentPlace()->getOffset();
    }

    uint32_t totalSize = renderer_.size();

    // Calculate remaining bytes
    uint32_t remainingBytes = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;

    // Check if we're close to the end of the buffer
    if (totalSize > 0 && remainingBytes < bufferThreshold_) {
        log_i("Near end of buffer: remaining=%" PRIu32 " bytes", remainingBytes);
        return true;
    }

    // For Kindle books, be careful about the EOF detection - we're ignoring the actual EOF flag
    // and only checking the buffer size remaining, since the EOF flag is unreliable for streaming
    // content
    if (remainingBytes < bufferThreshold_) {
        log_i("Near end of buffer: remaining=%" PRIu32 " bytes", remainingBytes);
        return true;
    }

    return false;
}

void KindleReadableScreen::turnPage() {
    if (atEnd_) {
        // If at end, don't advance (similar to ReadableScreen)
        log_i("At the end of content, not advancing page");
        return;
    }

    // No need to reset any flags here with our new approach

    // Save previous offset for logging
    uint32_t prevOffset = 0;
    if (currentPlace_) {
        prevOffset = currentPlace_->getOffset();
    }

    // CRITICAL EARLY CHECK: Check if we're about to cross a boundary when turning the page
    // This is done BEFORE we update currentPlace_
    if (newPlace_ && lastPreviewOffset_ > 0) {
        uint32_t newOffset = newPlace_->getOffset();

        // Multiple ways to detect a boundary crossing:
        // 1. Direct check if new position crosses boundary
        bool willCrossDirect = (prevOffset < lastPreviewOffset_ && newOffset >= lastPreviewOffset_);

        // 2. Check if current position plus page size would cross the boundary
        uint32_t pageSize =
            lastRenderedPageSize_ > 0 ? lastRenderedPageSize_ : 400; // Default if unknown
        bool willCrossBySize = (prevOffset + pageSize >= lastPreviewOffset_);

        // 3. Check if next place would show content past the boundary
        bool willShowContentPastBoundary = (newOffset >= lastPreviewOffset_);

        // 4. CRITICAL FIX: Look ahead one more page to see if that would cross boundary
        uint32_t nextNextOffset = 0;
        bool nextPageWillCrossBoundary = false;

        // Estimate the next page position by adding the page size to the new offset
        // This gives us an approximate location of where we'd be after another page turn
        nextNextOffset = newOffset + pageSize;

        // Check if the estimated future position would cross boundary
        nextPageWillCrossBoundary =
            (lastPreviewOffset_ > 0 && nextNextOffset >= lastPreviewOffset_ &&
             newOffset < lastPreviewOffset_);

        // Extra safety: if we're within one page size of the boundary, that's close enough to sync
        if (lastPreviewOffset_ > 0 && (lastPreviewOffset_ - newOffset) <= pageSize) {
            nextPageWillCrossBoundary = true;
        }

        log_i("[TURN PAGE ANALYSIS] prevOffset: %" PRIu32 ", newOffset: %" PRIu32
              ", nextNextOffset: %" PRIu32 ", boundary: %" PRIu32 ", pageSize: %" PRIu32
              ", willCrossDirect: %d, willCrossBySize: %d, willShowPastBoundary: %d, "
              "nextPageWillCross: %d",
              prevOffset, newOffset, nextNextOffset, lastPreviewOffset_, pageSize, willCrossDirect,
              willCrossBySize, willShowContentPastBoundary, nextPageWillCrossBoundary);

        // If ANY method indicates boundary crossing, mark for sync
        if (willCrossDirect || willCrossBySize || nextPageWillCrossBoundary ||
            (willShowContentPastBoundary && prevOffset < lastPreviewOffset_)) {
            log_i("[TURN PAGE CRITICAL] Turn WILL cross preview boundary - pre-emptively marking "
                  "for sync");
            // We want to ensure we notice this boundary crossing, so mark it
            needsNavigateSync_ = true;

            // If this is not after a preview and we're not already loading, sync immediately
            if (!lastCallWasPreview_ && !nextPageLoading_) {
                log_i("[TURN PAGE BOUNDARY FIX] Must sync immediately before completing turn");

                // Important: Keep current position before sync
                uint32_t currentPos = prevOffset;

                // Sync with server
                syncWithServer();

                // Don't continue with the turn if we've already synced
                if (currentPos < lastPreviewOffset_) {
                    log_i("[TURN PAGE BOUNDARY FIX] Already synced, can continue with turn");
                    // We can continue with the turn because we've already synced
                } else {
                    // No need to continue with the turn, nextPageLoading_ was already set
                    return;
                }
            }
            // CRITICAL FIX: Handle the case where we need to sync even after a preview
            // This fixes the scenario where we detect the page after the next will cross boundary
            else if (lastCallWasPreview_ && nextPageWillCrossBoundary && !nextPageLoading_) {
                // Calculate distance to boundary in pages
                int32_t distanceToBoundary = lastPreviewOffset_ - newOffset;
                int32_t pagesToBoundary =
                    distanceToBoundary > 0 ? (distanceToBoundary + pageSize - 1) / pageSize : 0;

                // FIXED: Never force sync here - we only want to force sync if we've ACTUALLY
                // crossed the boundary We'll just mark for sync instead, and it'll happen after the
                // user sees content
                log_i("[TURN PAGE DECISION] Marking for sync but not forcing it - %" PRId32
                      " pages from boundary",
                      pagesToBoundary);
                needsNavigateSync_ = true;

                // Log what we're doing differently
                if (pagesToBoundary <= 1) {
                    log_i("[TURN PAGE IMPROVEMENT] Previously would have forced sync when %" PRId32
                          " pages from boundary,"
                          " now allowing page turn to complete first",
                          pagesToBoundary);
                }
            }
        }
    }

    // Save current place
    if (newPlace_) {
        // Update the current place with the new place
        currentPlace_ = newPlace_;
        // Set the renderer's current place to the new place
        renderer_.setCurrentPlace(currentPlace_);

        // Get basic info for logging
        uint32_t newOffset = currentPlace_->getOffset();

        // Calculate and store the page size
        lastRenderedPageSize_ = (newOffset > prevOffset) ? (newOffset - prevOffset) : 0;

        // Log basic page turn information including boundary status
        log_i("[TURN PAGE] Turned page: offset %" PRIu32 " -> %" PRIu32 ", page size: %" PRIu32
              " bytes, preview boundary: %" PRIu32,
              prevOffset, newOffset, lastRenderedPageSize_, lastPreviewOffset_);

        // Add detailed WARNING logs to visualize proximity to boundary and decision logic
        if (lastPreviewOffset_ > 0) {
            int32_t distanceToBoundary = lastPreviewOffset_ - newOffset;
            int32_t pagesToBoundary =
                distanceToBoundary > 0
                    ? (distanceToBoundary + lastRenderedPageSize_ - 1) / lastRenderedPageSize_
                    : 0;

            // CRITICAL: Show how close we are to the boundary and why we would call /navigate-next
            log_w("[BOUNDARY PROXIMITY] Current offset: %" PRIu32 ", boundary: %" PRIu32
                  ", distance: %" PRId32 " bytes (~%" PRId32 " pages), lastCallWasPreview: %d",
                  newOffset, lastPreviewOffset_, distanceToBoundary, pagesToBoundary,
                  lastCallWasPreview_);

            // Show next page prediction
            uint32_t nextTurnOffset = newOffset + lastRenderedPageSize_;
            bool wouldCrossNextTurn = nextTurnOffset >= lastPreviewOffset_;
            log_w("[BOUNDARY PREDICTION] Next turn would reach: %" PRIu32 " (boundary: %" PRIu32
                  "), "
                  "would cross boundary: %d, needs sync: %d",
                  nextTurnOffset, lastPreviewOffset_, wouldCrossNextTurn, needsNavigateSync_);

            // Critical decision rule explanation
            if (needsNavigateSync_) {
                log_w("[BOUNDARY DECISION] Need to call /navigate-next before any more "
                      "/preview-next calls");
            } else if (wouldCrossNextTurn) {
                log_w("[BOUNDARY DECISION] Next turn would cross boundary, should call "
                      "/navigate-next now");
            } else if (pagesToBoundary <= 2) {
                log_w("[BOUNDARY DECISION] Very close to boundary (%" PRId32
                      " pages), consider calling /navigate-next",
                      pagesToBoundary);
            }
        }

        // Double-check if we've crossed a boundary now that we've updated our position
        if (lastPreviewOffset_ > 0 && newOffset >= lastPreviewOffset_ &&
            prevOffset < lastPreviewOffset_) {
            log_i("[TURN PAGE] Just crossed preview boundary at %" PRIu32
                  " during this turn! (lastCallWasPreview_=%d)",
                  lastPreviewOffset_, lastCallWasPreview_);

            // CRITICAL CHANGE: If we've ACTUALLY crossed the boundary during this turn,
            // we MUST sync with the server regardless of lastCallWasPreview_
            if (!nextPageLoading_) {
                log_i("[BOUNDARY CRITICAL OVERRIDE] Just crossed boundary during turn - MUST sync "
                      "now");

                // This is a special case where we MUST potentially break alternation
                // to maintain server sync when we've definitely crossed a boundary
                syncWithServer();
                return;
            } else {
                // If we're already loading, just mark for sync
                log_i("[BOUNDARY OVERRIDE] Loading in progress, marking for future sync");
                needsNavigateSync_ = true;
            }
        }
    } else {
        // This shouldn't happen, but handle it gracefully
        currentPlace_ = renderer_.getCurrentPlace();
        uint32_t newOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
        log_i("[TURN PAGE] No new place available, using current position: %" PRIu32
              ", preview boundary: %" PRIu32,
              newOffset, lastPreviewOffset_);
    }

    // Check if we're at the end - for Kindle books we respect EOF only when we really want to
    // (the renderer is configured to ignore EOF during initialization)
    atEnd_ = renderer_.shouldRespectEOF();

    // Only log if we've reached the end
    if (atEnd_) {
        log_i("Reached end of content");
    }
}

// This method is no longer needed since we handle the response directly in each method

void KindleReadableScreen::clearScreenForContent() {
    // Clear any message views
    loadingMessageView_.setText("");
    errorMessageView_.setText("");

    // Set frame and layout for rendering
    setFrame(defaultFrameRect());
    setNeedsLayout();

    // First clear the display
    display_.clearDisplay();

    // Draw a clean background
    draw();

    log_i("Screen cleared for rendering Kindle content");
}

void KindleReadableScreen::disconnectNetworkConnections() {
    // Cancel any ongoing network operations
    if (cancelFlag_) {
        CancelFlag::cancel(cancelFlag_);
        cancelFlag_ = nullptr;
    }

    // Disconnect WiFi if connected
    if (WiFi::getInstance().connected()) {
        WiFi::getInstance().disconnectWiFi(3000);
    }

#if CONFIG_USE_BLE_SERVER_PROXY
    // Check if BLE manager exists
    auto bleManager = BLEManager::getInstance(false); // Don't create it if it doesn't exist
    if (bleManager) {
        // Add a safety check to see if it's connected first
        if (bleManager->connected()) {
            log_i("Disconnecting BLE proxy");
            bleManager->disconnectProxy();
        } else {
            log_i("BLE proxy not connected, skipping disconnection");
        }
    } else {
        log_i("BLE manager not available, skipping disconnection");
    }
#endif
}

void KindleReadableScreen::dismissScreen() {
    if (dismissed_) {
        log_i("KindleReadableScreen already dismissed, ignoring dismissScreen call");
        return;
    }

    log_i("KindleReadableScreen being dismissed");
    dismissed_ = true;

    // Cancel any pending auto-dismiss task
    if (autoDismissCancelFlag_) {
        log_i("Cancelling pending auto-dismiss task");
        CancelFlag::cancel(autoDismissCancelFlag_);
        autoDismissCancelFlag_ = nullptr;
    }

    // Cancel any pending delayed message
    cancelDelayedLoadingMessage();

    // Disconnect network connections - already has defensive error handling
    disconnectNetworkConnections();

    // Clear any ongoing state
    state_ = State::IDLE;
    currentPage_ = 0;
    totalPages_ = 0;
    currentPageText_.clear();
    currentPageImageUrl_.clear();

    // Clear message views
    loadingMessageView_.setText("");
    errorMessageView_.setText("");
    showLoadingWithRoundedRect_ = false;
    setNeedsRerender();

    // Reset renderer safely without exceptions
    // Check if the tokenizer is initialized before trying to reset it
    if (renderer_.getStream()) {
        // Reset the renderer's ignore EOF setting
        renderer_.setIgnoreEOF(false);

        // Create an empty string for the stream
        std::string emptyStr;
        auto emptyStream = std::make_shared<StdStringStream>(emptyStr, DocType::TXT);

        // Set the stream - if this fails, we just continue
        // We don't use try/catch since exceptions are disabled
        renderer_.setStream(emptyStream);
        log_i("Reset renderer stream to empty");
    }

    // Always attempt to go back to the previous screen, even if there were errors
    back();
}

auto KindleReadableScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    // For non-content states, center the message views
    if (state_ != State::DISPLAY_CONTENT) {
        // log_d("Layouting KindleReadableScreen in non-content state");
        const auto inset = 2;
        auto contentFrame = SolRect::fromSize(size).inset(inset);

        // Center the message views vertically and horizontally
        if (state_ == State::ERROR) {
            context.setViewFrame(errorMessageView_, contentFrame,
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                                     LayoutOptionsAlignCenterVertical);
        } else {
            // Position the main loading message in the center
            context.setViewFrame(loadingMessageView_, contentFrame,
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                                     LayoutOptionsAlignCenterVertical);

            // Position the delayed message below the main message if it's visible
            if (!delayedMessageView_.isHidden()) {
                // Place the delayed message in the same content frame
                // but offset it towards the bottom half
                SolRect delayedFrame = contentFrame;
                // Just use a fixed position in the lower portion of the screen
                context.setViewFrame(delayedMessageView_, delayedFrame,
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                                         LayoutOptionsAlignBottom);
            }
        }
    } else {
        // log_d("Layouting KindleReadableScreen in content state");
    }

    return size;
}

void KindleReadableScreen::drawInRect(SolRect rect) {
    // For non-content states, draw the appropriate message view
    if (state_ != State::DISPLAY_CONTENT) {
        switch (state_) {
        case State::CONNECTING:
        case State::LOADING_CONTENT:
            // If we're showing the rounded rect for loading
            if (showLoadingWithRoundedRect_ && !loadingMessageView_.isHidden()) {
                // Calculate the bounds of the loading message text
                int maxWidth = 0;
                const auto &text = loadingMessageView_.getText();
                if (!text.empty()) {
                    maxWidth = display_.getTextWidth(text, loadingMessageView_.getFont());
                }

                // Add padding around the text
                const int padding = 12;
                int lineHeight = loadingMessageView_.getFont()->lineHeight();

                // Get the position from the parent's layout context
                SolRect screenRect = layoutContext_.screenFrame();

                // Calculate perfect center position in the screen
                int centerY = screenRect.origin.y + screenRect.size.height / 2;

                // Create rectangle for the background with enough padding for good looks
                SolRect bgRect = SolRect::zero();
                if (loadingMessageView_.alignCenter) {
                    bgRect.origin.x =
                        screenRect.origin.x + (screenRect.size.width - maxWidth) / 2 - padding;
                    bgRect.size.width = maxWidth + padding * 2;
                } else {
                    bgRect.origin.x = screenRect.origin.x - padding;
                    bgRect.size.width = maxWidth + padding * 2;
                }

                // Make sure we have enough vertical space in the rectangle
                // (lineHeight + padding on top and bottom)
                bgRect.size.height = lineHeight + padding * 2;

                // Position background rect vertically centered on the screen
                bgRect.origin.y = centerY - bgRect.size.height / 2;

                // Add debug logging to help diagnose centering issues
                log_i("Loading text: '%s', text width: %d, bgRect: x=%d, y=%d, w=%d, h=%d",
                      loadingMessageView_.getText().c_str(), maxWidth, bgRect.origin.x,
                      bgRect.origin.y, bgRect.size.width, bgRect.size.height);

                // Create a completely new approach - set frame to the size of the rect
                SolRect loadingFrame = bgRect;

                // Ensure alignment is centered
                loadingMessageView_.alignCenter = true;

                // Reposition the loading message view to be exactly centered in the rect
                loadingMessageView_.setFrame(loadingFrame);

                // Draw filled rounded rectangle with a white fill and black outline
                // First fill with white background
                display_.fillRoundRect(bgRect.origin.x, bgRect.origin.y, bgRect.size.width,
                                       bgRect.size.height, 10,
                                       display_.bg); // Corner radius 10, white background

                // Then draw black outline
                display_.drawRoundRect(bgRect.origin.x, bgRect.origin.y, bgRect.size.width,
                                       bgRect.size.height, 10,
                                       display_.fg); // Corner radius 10, black outline

                // Draw the loading message with normal colors (black text on white background)
                loadingMessageView_.draw();
            } else {
                // Draw normally without rounded rect
                loadingMessageView_.draw();
            }

            // Also draw the delayed message if it's visible
            if (!delayedMessageView_.isHidden()) {
                delayedMessageView_.draw();
            }
            break;

        case State::ERROR:
            errorMessageView_.draw();
            break;

        default:
            break;
        }
    }
}

void KindleReadableScreen::showPopupMenu() {
    if (currentScreen_ == popupMenuScreen_) {
        return;
    }

    // Create a readable-like object for the popup menu to use
    auto readablePlaceholder = std::make_shared<Readable>();
    readablePlaceholder->title = kindleBook_->title;

    // Add authors from kindle book to readable
    Author author;
    author.name = kindleBook_->author;
    readablePlaceholder->authors.push_back(author);

    readablePlaceholder->place = currentPlace_; // Use current reading position

    // Show the popup menu - no need to set readable since that's only needed for some features
    setCurrentScreen(popupMenuScreen_);
    setNeedsRerender();
}

void KindleReadableScreen::scheduleDelayedLoadingMessage() {
    // Cancel any existing delayed message
    cancelDelayedLoadingMessage();

    // Create a new cancel flag for this delayed message
    delayedMessageCancelFlag_ = std::make_shared<CancelFlag>();

    // Capture a copy of the flag for the lambda
    std::shared_ptr<CancelFlag> localFlag = delayedMessageCancelFlag_;

    // Schedule a message to appear after 10 seconds if content is still loading
    DispatchGetCore0BackgroundQueue()->dispatch([this, localFlag]() {
        // Wait for 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));

        // Check if the task was cancelled
        if (!localFlag || CancelFlag::isCancelled(localFlag)) {
            log_i("Delayed loading message was cancelled");
            return;
        }

        // Display the message on the UI thread
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_ || state_ != State::LOADING_CONTENT) {
                return; // Don't show if we're no longer loading or screen was dismissed
            }

            // Set and show the delayed message
            delayedMessageView_.setText("This can take up to 30 seconds.");
            delayedMessageView_.setHidden(false);

            // Call setNeedsLayout to recalculate positions of all views
            setNeedsLayout();

            display_.clearDisplay();
            delayedMessageView_.draw();
            draw();
            display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true);

            log_i("Showing delayed loading message after 10 seconds");
        });
    });
}

void KindleReadableScreen::cancelDelayedLoadingMessage() {
    // Cancel any pending delayed message
    if (delayedMessageCancelFlag_) {
        CancelFlag::cancel(delayedMessageCancelFlag_);
        delayedMessageCancelFlag_ = nullptr;

        // Hide the delayed message view if it's visible
        if (!delayedMessageView_.isHidden()) {
            delayedMessageView_.setHidden(true);
            log_i("Cancelled and hid delayed loading message");
        }
    }

    // Reset the rounded rect flag when canceling messages
    showLoadingWithRoundedRect_ = false;
}

void KindleReadableScreen::loadNextContent() {
    // Only load more if we're below buffer threshold
    if (!checkNearEndOfBuffer()) {
        return;
    }

    // Check if we've crossed a boundary - this takes precedence over alternation
    uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
    bool hasCrossedBoundary = (currentOffset >= lastPreviewOffset_ && lastPreviewOffset_ > 0);

    if (hasCrossedBoundary) {
        // MUST sync with server if we've crossed a boundary, regardless of last call type
        log_i("Loading content: BOUNDARY CROSSED (position %" PRIu32 " >= boundary %" PRIu32
              ") - Must sync with server",
              currentOffset, lastPreviewOffset_);
        syncWithServer();
        return;
    }

    // Normal case: Always respect alternation pattern between preview and navigate
    if (lastCallWasPreview_) {
        // Last call was preview, we need to sync with server now
        log_i("Loading content: Syncing with server first (lastCallWasPreview_=true)");
        syncWithServer();
    } else {
        // Last call was navigate, load preview next
        log_i("Loading content: Fetching preview (lastCallWasPreview_=false)");
        loadNextKindlePreview();
    }
}

void KindleReadableScreen::syncWithServer() {
    // Only initiate one navigate request at a time
    if (nextPageLoading_) {
        log_i("Already syncing with server, skipping duplicate request");
        return;
    }

    uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
    uint32_t preSyncBoundary = lastPreviewOffset_;
    bool preSyncFlag = needsNavigateSync_;

    log_i("[SYNC] Starting server sync - current position: %" PRIu32 ", preview boundary: %" PRIu32
          ", needsNavigateSync: %d, lastCallWasPreview: %d",
          currentOffset, preSyncBoundary, preSyncFlag, lastCallWasPreview_);

    nextPageLoading_ = true;

    DispatchGetCore0BackgroundQueue()->dispatch([this, preSyncBoundary, currentOffset]() {
        auto response = std::make_shared<KindlePageResponse>();
        auto sindarinApi = SindarinAPI::getInstance();

        // Call navigateKindleNext with ocr=false to sync server state without getting content
        int httpCode = sindarinApi->navigateKindleNext(response.get(), cancelFlag_, useBle_, false);

        DispatchGetUiQueue()->dispatch(
            [this, httpCode, response, preSyncBoundary, currentOffset]() {
            nextPageLoading_ = false;
            justNavigatedToServer_ = true;

            // CRITICAL: Mark that the last API call was a navigate
            lastCallWasPreview_ = false;
            log_i("[SYNC] Server sync completed with HTTP code %d", httpCode);

            if (httpCode != 200) {
                log_w("[SYNC ERROR] Failed to sync server state (HTTP code %d)", httpCode);
            } else {
                log_i("[SYNC SUCCESS] Pre-sync state: boundary = %" PRIu32 ", position = %" PRIu32
                      ", needsNavigateSync = %d",
                      preSyncBoundary, currentOffset, needsNavigateSync_);

                // Clear the sync flag first
                needsNavigateSync_ = false;

                // CRITICAL: Reset the preview boundary to 0 after successful syncing
                // This prevents us from re-syncing against the same boundary
                uint32_t oldBoundary = lastPreviewOffset_;
                lastPreviewOffset_ = 0;

                log_i("[SYNC] Reset preview boundary (%" PRIu32
                      " -> 0) and cleared sync flag after successful sync",
                      oldBoundary);

                // Check if we need to proactively load more content due to short pages
                uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
                uint32_t totalSize = renderer_.size();
                uint32_t remainingBytes =
                    (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;

                // Only load more if we're critically low on buffer space (well below threshold)
                if (remainingBytes < bufferThreshold_ / 2 && !previewRequestInProgress_) {
                    log_i("Low buffer after sync, loading more content");
                    // Small delay to ensure server state is fully processed
                    DispatchGetUiQueue()->dispatch([this]() {
                        if (!dismissed_) {
                            // Small delay to avoid immediate execution
                            vTaskDelay(pdMS_TO_TICKS(100));
                            // Will load just one more page, respecting alternation
                            loadNextContent();
                        }
                    });
                }

                // Update book metadata but NOT the text content
                if (response && response->success) {
                    // Update current page information
                    currentPage_ = response->progress.currentPage;
                    totalPages_ = response->progress.totalPages;
                    currentPageImageUrl_ = response->screenshotUrl;

                    // Update the book progress in the KindleBook object
                    kindleBook_->progressPercent = response->progress.percentage;
                    kindleBook_->progress = StringFormat("%d%%", response->progress.percentage);

                    log_i("Updated book progress: page %d/%d (%d%%)", currentPage_, totalPages_,
                          response->progress.percentage);

                    // After navigation, we can queue up the next preview with a short delay
                    // This is safe because we just set lastCallWasPreview_ = false above
                    DispatchGetUiQueue()->dispatch([this]() {
                        if (!dismissed_ && !previewRequestInProgress_) {
                            // Short delay to ensure server state is fully processed
                            vTaskDelay(pdMS_TO_TICKS(50));
                            // Double-check conditions haven't changed during delay
                            if (!dismissed_ && !previewRequestInProgress_ && !lastCallWasPreview_) {
                                log_i("Loading next preview after sync");
                                loadNextKindlePreview();
                            }
                        }
                    });
                }
            }
        });
    });
}

// --- Add the new method definition here ---
void KindleReadableScreen::logPageContext(uint32_t offsetBeforeTurn, uint32_t offsetAfterTurn) {
    if (currentPlace_ && renderer_.getStream()) {
        auto stream = renderer_.getStream();
        size_t streamSize = stream->size();
        if (streamSize > 0) {
            std::string currentPageContent;
            std::string previewContent;

            // Read current page content
            size_t readStart = offsetBeforeTurn;
            size_t readEnd = std::min(static_cast<size_t>(offsetAfterTurn),
                                      streamSize); // Read up to the end of the turned page
            if (readEnd > readStart) {
                currentPageContent.resize(readEnd - readStart);
                stream->seek(readStart);
                stream->readBytes(reinterpret_cast<uint8_t *>(currentPageContent.data()),
                                  readEnd - readStart);
            }

            // Read start of preview content if boundary exists
            if (lastPreviewOffset_ > 0 && lastPreviewOffset_ < streamSize) {
                size_t previewReadStart = lastPreviewOffset_;
                size_t previewReadEnd =
                    std::min(streamSize, previewReadStart + 200); // Read ~200 chars for 10 words
                if (previewReadEnd > previewReadStart) {
                    previewContent.resize(previewReadEnd - previewReadStart);
                    stream->seek(previewReadStart);
                    stream->readBytes(reinterpret_cast<uint8_t *>(previewContent.data()),
                                      previewReadEnd - previewReadStart);
                }
            }

            log_i("[TEXT DEBUG] Page Start (Offset %" PRIu32 "): '%s...'", offsetBeforeTurn,
                  GetNWords(currentPageContent, 10, true).c_str());
            log_i("[TEXT DEBUG] Page End   (Offset %" PRIu32 "): '...%s'", offsetAfterTurn,
                  GetNWords(currentPageContent, 10, false).c_str());
            if (lastPreviewOffset_ > 0) {
                log_i("[TEXT DEBUG] Preview Start (Boundary %" PRIu32 "): '%s...'",
                      lastPreviewOffset_, GetNWords(previewContent, 10, true).c_str());
            }
        }
        // Restore stream position (important!)
        stream->seek(offsetAfterTurn);
    }
}
// --- End of new method definition ---
