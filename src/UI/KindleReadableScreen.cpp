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
    log_i("Created KindleReadableScreen for book: %s by %s", kindleBook->title.c_str(),
          kindleBook->author.c_str());

    // Initialize the page manager
    pageManager_ = std::make_shared<KindlePageManager>(kindleBook);

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

        // Initial open book call is handled by pageManager_
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
        justNavigatedToServer_ = false;
        // KindlePageManager handles the API state tracking

        // Log clear debug info about the initial state
        uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
        uint32_t totalSize = renderer_.size();
        log_i("[INITIAL LOAD] Current position: %" PRIu32 ", buffer size: %" PRIu32
              ", preview boundary: %" PRIu32 ", lastCallWasPreview: %d",
              currentOffset, totalSize, pageManager_->getPreviewBoundary(),
              pageManager_->wasLastCallPreview());

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
                if (!dismissed_ && !pageManager_->isPreviewRequestInProgress() &&
                    !pageManager_->wasLastCallPreview()) {
                    log_i("[INITIAL LOAD] Starting first preview request after delay");
                    // The pageManager will handle tracking the API call state
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
    if (dismissed_) {
        return;
    }

    // Update UI state
    setState(State::LOADING_BACKGROUND);

    // Create a cancel flag if needed
    if (!cancelFlag_) {
        cancelFlag_ = std::make_shared<CancelFlag>();
    }

    // Load the preview using the page manager
    pageManager_->loadNextPreview(currentPlace_, cancelFlag_, useBle_,
                                  [this](bool success,
                                         const std::shared_ptr<KindlePageResponse> &response,
                                         const std::string &errorMessage) {
        if (dismissed_) {
            return;
        }

        if (!success) {
            log_w("Failed to load preview: %s", errorMessage.c_str());
            setState(State::DISPLAY_CONTENT);
            return;
        }

        // log_i("Preview loaded successfully, text size: %zu bytes", response->ocrText.length());

        // Append the preview text to the current text
        currentPageText_ += response->ocrText;

        // Create a stream with the updated text content
        auto stream = std::make_shared<StdStringStream>(currentPageText_, DocType::TXT);

        // Save the current position before updating the stream
        uint32_t savedOffset = currentPlace_ ? currentPlace_->getOffset() : 0;

        // Update the renderer with the new stream
        renderer_.setStream(stream);

        // Restore the place after setting the stream
        if (currentPlace_ && savedOffset > 0) {
            auto restoredPlace = std::dynamic_pointer_cast<TxtPlace>(Place::factory(savedOffset));
            currentPlace_ = restoredPlace;
            renderer_.setCurrentPlace(currentPlace_);
            log_i("Restored place to offset %" PRIu32 " after loading preview", savedOffset);
        }

        // Update the UI state
        setState(State::DISPLAY_CONTENT);

        // Check if we need to redraw the current page
        uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
        uint32_t totalSize = renderer_.size();
        uint32_t remainingBytes = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;
        bool wasNearEndOfBuffer = remainingBytes < bufferThreshold_;

        if (wasNearEndOfBuffer || needsRedrawAfterLoad_) {
            // log_i("Refreshing page after preview load");

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

            // Reset the redraw flag
            needsRedrawAfterLoad_ = false;
        }
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
            if (!nextPageLoading_) {
                loadNextContent(); // Handles content loading using the page manager
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

    log_i("[NAVIGATE LOCAL] Turned page: %" PRIu32 " -> %" PRIu32, offsetBeforeTurn,
          offsetAfterTurn);

    // --- Boundary Detection & Sync Decision ---

    // Check if we've crossed a boundary
    bool justCrossedBoundary = pageManager_->hasCrossedPreviewBoundary(offsetAfterTurn) &&
                               !pageManager_->hasCrossedPreviewBoundary(offsetBeforeTurn);

    // Check if the next turn would cross a boundary
    bool nextTurnWillCrossBoundary =
        pageManager_->wouldNextTurnCrossBoundary(offsetAfterTurn, pageSize);

    // Log boundary status
    uint32_t previewBoundary = pageManager_->getPreviewBoundary();
    log_i("[NAVIGATE PREDICT] Current: %" PRIu32 ", boundary: %" PRIu32
          ", justCrossed: %d, nextWillCross: %d",
          offsetAfterTurn, previewBoundary, justCrossedBoundary, nextTurnWillCrossBoundary);

    // Determine if we need to sync with the server now
    bool shouldSyncNow = false;

    // If we just crossed a boundary, we need to sync immediately
    if (justCrossedBoundary) {
        log_i("[SYNC DECISION] Just crossed boundary. Syncing immediately.");
        shouldSyncNow = true;
    }
    // If the next turn will cross a boundary and we just loaded a preview, sync now
    else if (nextTurnWillCrossBoundary && pageManager_->wasLastCallPreview()) {
        log_i("[SYNC DECISION] Next turn will cross boundary. Syncing proactively.");
        shouldSyncNow = true;
    }
    // Otherwise, check if we need to sync based on alternation pattern
    else if (pageManager_->needsNavigateCall(offsetAfterTurn) && !nextPageLoading_) {
        log_i("[SYNC DECISION] Need to navigate based on alternation pattern.");
        shouldSyncNow = true;
    }

    // Sync with the server if needed
    if (shouldSyncNow && !nextPageLoading_) {
        syncWithServer();
    }

    // --- Background Loading Check ---

    // Check if we need to load more content in the background after the turn
    uint32_t remainingBytesAfterTurn =
        (offsetAfterTurn < totalSize) ? (totalSize - offsetAfterTurn) : 0;
    if (remainingBytesAfterTurn < bufferThreshold_ && !nextPageLoading_ &&
        !pageManager_->isPreviewRequestInProgress() && !pageManager_->wasLastCallPreview()) {
        log_i("[NAVIGATE LOAD] Near end of buffer (remaining: %" PRIu32
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

            // KindlePageManager will handle the API state tracking

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
    if (newPlace_ && pageManager_->getPreviewBoundary() > 0) {
        uint32_t newOffset = newPlace_->getOffset();
        uint32_t previewBoundary = pageManager_->getPreviewBoundary();

        // Multiple ways to detect a boundary crossing:
        // 1. Direct check if new position crosses boundary
        bool willCrossDirect = (prevOffset < previewBoundary && newOffset >= previewBoundary);

        // 2. Check if current position plus page size would cross the boundary
        uint32_t pageSize =
            lastRenderedPageSize_ > 0 ? lastRenderedPageSize_ : 400; // Default if unknown
        bool willCrossBySize = (prevOffset + pageSize >= previewBoundary);

        // 3. Check if next place would show content past the boundary
        bool willShowContentPastBoundary = (newOffset >= previewBoundary);

        // 4. CRITICAL FIX: Look ahead one more page to see if that would cross boundary
        uint32_t nextNextOffset = 0;
        bool nextPageWillCrossBoundary = false;

        // Estimate the next page position by adding the page size to the new offset
        // This gives us an approximate location of where we'd be after another page turn
        nextNextOffset = newOffset + pageSize;

        // Check if the estimated future position would cross boundary
        nextPageWillCrossBoundary = (previewBoundary > 0 && nextNextOffset >= previewBoundary &&
                                     newOffset < previewBoundary);

        // Extra safety: if we're within one page size of the boundary, that's close enough to sync
        if (previewBoundary > 0 && (previewBoundary - newOffset) <= pageSize) {
            nextPageWillCrossBoundary = true;
        }

        log_i("[TURN PAGE ANALYSIS] prevOffset: %" PRIu32 ", newOffset: %" PRIu32
              ", nextNextOffset: %" PRIu32 ", boundary: %" PRIu32 ", pageSize: %" PRIu32
              ", willCrossDirect: %d, willCrossBySize: %d, willShowPastBoundary: %d, "
              "nextPageWillCross: %d",
              prevOffset, newOffset, nextNextOffset, previewBoundary, pageSize, willCrossDirect,
              willCrossBySize, willShowContentPastBoundary, nextPageWillCrossBoundary);

        // If ANY method indicates boundary crossing, mark for sync
        if (willCrossDirect || willCrossBySize || nextPageWillCrossBoundary ||
            (willShowContentPastBoundary && prevOffset < previewBoundary)) {
            log_i("[TURN PAGE CRITICAL] Turn WILL cross preview boundary - pre-emptively marking "
                  "for sync");
            // The KindlePageManager will track whether we need to sync

            // If this is not after a preview and we're not already loading, sync immediately
            if (!pageManager_->wasLastCallPreview() && !nextPageLoading_) {
                log_i("[TURN PAGE BOUNDARY FIX] Must sync immediately before completing turn");

                // Important: Keep current position before sync
                uint32_t currentPos = prevOffset;

                // Sync with server
                syncWithServer();

                // Don't continue with the turn if we've already synced
                if (currentPos < previewBoundary) {
                    log_i("[TURN PAGE BOUNDARY FIX] Already synced, can continue with turn");
                    // We can continue with the turn because we've already synced
                } else {
                    // No need to continue with the turn, nextPageLoading_ was already set
                    return;
                }
            }
            // CRITICAL FIX: Handle the case where we need to sync even after a preview
            // This fixes the scenario where we detect the page after the next will cross boundary
            else if (pageManager_->wasLastCallPreview() && nextPageWillCrossBoundary &&
                     !nextPageLoading_) {
                // Calculate distance to boundary in pages
                int32_t distanceToBoundary = previewBoundary - newOffset;
                int32_t pagesToBoundary =
                    distanceToBoundary > 0 ? (distanceToBoundary + pageSize - 1) / pageSize : 0;

                // FIXED: Never force sync here - we only want to force sync if we've ACTUALLY
                // crossed the boundary We'll just mark for sync instead, and it'll happen after the
                // user sees content
                log_i("[TURN PAGE DECISION] Marking for sync but not forcing it - %" PRId32
                      " pages from boundary",
                      pagesToBoundary);
                // No need to set a flag now, we'll check with pageManager_ when needed

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
              prevOffset, newOffset, lastRenderedPageSize_, pageManager_->getPreviewBoundary());

        // Add detailed WARNING logs to visualize proximity to boundary and decision logic
        uint32_t previewBoundary = pageManager_->getPreviewBoundary();
        if (previewBoundary > 0) {
            int32_t distanceToBoundary = previewBoundary - newOffset;
            int32_t pagesToBoundary =
                distanceToBoundary > 0
                    ? (distanceToBoundary + lastRenderedPageSize_ - 1) / lastRenderedPageSize_
                    : 0;

            // CRITICAL: Show how close we are to the boundary and why we would call /navigate-next
            log_w("[BOUNDARY PROXIMITY] Current offset: %" PRIu32 ", boundary: %" PRIu32
                  ", distance: %" PRId32 " bytes (~%" PRId32 " pages), lastCallWasPreview: %d",
                  newOffset, previewBoundary, distanceToBoundary, pagesToBoundary,
                  pageManager_->wasLastCallPreview());

            // Show next page prediction
            uint32_t nextTurnOffset = newOffset + lastRenderedPageSize_;
            bool wouldCrossNextTurn = nextTurnOffset >= previewBoundary;
            bool needsNavigate = pageManager_->needsNavigateCall(newOffset);
            log_w("[BOUNDARY PREDICTION] Next turn would reach: %" PRIu32 " (boundary: %" PRIu32
                  "), "
                  "would cross boundary: %d, needs sync: %d",
                  nextTurnOffset, previewBoundary, wouldCrossNextTurn, needsNavigate);

            // Critical decision rule explanation
            if (needsNavigate) {
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
        if (previewBoundary > 0 && newOffset >= previewBoundary && prevOffset < previewBoundary) {
            log_i("[TURN PAGE] Just crossed preview boundary at %" PRIu32
                  " during this turn! (lastCallWasPreview_=%d)",
                  previewBoundary, pageManager_->wasLastCallPreview());

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
                // If we're already loading, just log the situation
                log_i("[BOUNDARY OVERRIDE] Loading in progress, will need future sync");
            }
        }
    } else {
        // This shouldn't happen, but handle it gracefully
        currentPlace_ = renderer_.getCurrentPlace();
        uint32_t newOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
        log_i("[TURN PAGE] No new place available, using current position: %" PRIu32
              ", preview boundary: %" PRIu32,
              newOffset, pageManager_->getPreviewBoundary());
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

    // Get current position
    uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;

    // Use the page manager to decide what to load next
    if (pageManager_->needsNavigateCall(currentOffset)) {
        // We need to sync with the server
        log_i("Loading content: Syncing with server");
        syncWithServer();
    } else {
        // Load the next preview
        log_i("Loading content: Fetching preview");
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

    log_i("[SYNC] Starting server sync - current position: %" PRIu32, currentOffset);

    nextPageLoading_ = true;

    // Create a cancel flag if needed
    if (!cancelFlag_) {
        cancelFlag_ = std::make_shared<CancelFlag>();
    }

    // Use the page manager to sync with the server
    pageManager_->syncWithServer(cancelFlag_, useBle_,
                                 [this](bool success,
                                        const std::shared_ptr<KindlePageResponse> &response,
                                        const std::string &errorMessage) {
        nextPageLoading_ = false;

        if (!success) {
            log_w("[SYNC ERROR] Failed to sync server state: %s", errorMessage.c_str());
            return;
        }

        log_i("[SYNC SUCCESS] Server sync completed successfully");

        // Check if we need to proactively load more content due to short pages
        uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
        uint32_t totalSize = renderer_.size();
        uint32_t remainingBytes = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;

        // Update current page information
        if (response) {
            currentPage_ = response->progress.currentPage;
            totalPages_ = response->progress.totalPages;
            currentPageImageUrl_ = response->screenshotUrl;

            log_i("Updated book progress: page %d/%d (%d%%)", currentPage_, totalPages_,
                  response->progress.percentage);
        }

        // Only load more if we're critically low on buffer space (well below threshold)
        if (remainingBytes < bufferThreshold_ / 2) {
            log_i("Low buffer after sync, loading more content");
            // Small delay to ensure server state is fully processed
            DispatchGetUiQueue()->dispatch([this]() {
                if (!dismissed_) {
                    // Small delay to avoid immediate execution
                    vTaskDelay(pdMS_TO_TICKS(100));
                    // Load the next preview
                    loadNextKindlePreview();
                }
            });
        } else {
            // After navigation, we can queue up the next preview with a short delay
            DispatchGetUiQueue()->dispatch([this]() {
                if (!dismissed_) {
                    // Short delay to ensure server state is fully processed
                    vTaskDelay(pdMS_TO_TICKS(50));
                    // Load the next preview
                    loadNextKindlePreview();
                }
            });
        }
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
            uint32_t previewBoundary = pageManager_->getPreviewBoundary();
            if (previewBoundary > 0 && previewBoundary < streamSize) {
                size_t previewReadStart = previewBoundary;
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
                  getNWords(currentPageContent, 10, false).c_str());
            if (previewBoundary > 0) {
                log_i("[TEXT DEBUG] Preview Start (Boundary %" PRIu32 "): '%s...'", previewBoundary,
                      getNWords(previewContent, 10, true).c_str());
            }
        }
        // Restore stream position (important!)
        stream->seek(offsetAfterTurn);
    }
}
// --- End of new method definition ---
