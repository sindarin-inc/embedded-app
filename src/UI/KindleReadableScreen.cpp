#include "UI/KindleReadableScreen.hpp"

#include <cinttypes>
#include <inttypes.h>

#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEManager.hpp"
#endif
#include "Misc/StringUtil.hpp"
#include "Models/Readable.hpp"
#include "UI/Fonts/Fonts.hpp"
#include "sindarin-debug.h"

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
      errorMessageView_(display, 5) {
    log_i("Created KindleReadableScreen for book: %s by %s", kindleBook->title.c_str(),
          kindleBook->author.c_str());

    // Since we don't have a standard Readable for Kindle books,
    // we need to set up some of our state differently
    firstKindleLoad_ = true;

    // Set up loading message
    loadingMessageView_.setText("Loading...");
    loadingMessageView_.setFont(font);
    loadingMessageView_.alignCenter = true;

    // Set up error message
    errorMessageView_.setFont(font);
    errorMessageView_.alignCenter = true;

    // Initialize network connection
    setState(State::CONNECTING);
    
    // Initialize the popup menu
    auto popupNavigationFactory = [this]() {
        std::shared_ptr<std::vector<std::shared_ptr<MenuItem>>> menuItems = 
            std::make_shared<std::vector<std::shared_ptr<MenuItem>>>();

        menuItems->emplace_back(std::make_shared<MenuItemSingleLine>(
            "Back to Text", [this]() {
                display_.clearDisplay();
                this->setCurrentScreen(nullptr);
                setNeedsRerender();
            }));

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
        display, prefs_, fileSystem_, readablePlaceholder, lightingController_, renderer_, telemetry_, localStore_, stream_);

    popupMenuScreen_->addTab("Popup", popupNavigationFactory, 8);
    popupMenuScreen_->populateMenuView();
}

void KindleReadableScreen::render(DisplaySystem &display) {
    // If we have a popup menu or other screen, let it handle rendering
    if (currentScreen_) {
        currentScreen_->render(display);
        return;
    }

    if (state_ == State::DISPLAY_CONTENT || state_ == State::LOADING_BACKGROUND) {
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
            uint32_t remainingBytes = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;
            
            // Calculate and store the page size
            lastRenderedPageSize_ = (nextOffset > currentOffset) ? (nextOffset - currentOffset) : 0;

            log_i("BUFFER INFO - Reading at offset: %" PRIu32 "/%" PRIu32 " (%.1f%%), remaining: %" PRIu32 " bytes, next "
                  "offset: %" PRIu32 ", page size: %" PRIu32 " bytes",
                  currentOffset, totalSize,
                  (totalSize > 0) ? (currentOffset * 100.0f / totalSize) : 0, remainingBytes,
                  nextOffset, lastRenderedPageSize_);
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
                loadingMessageView_.draw();
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
        errorMessageView_.draw();
        break;

    default:
        break;
    }

    // Handle display updates
    if (firstKindleLoad_) {
        log_w("RENDER: Updating display to full on first Kindle book load");
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
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

    // If in an error state, any button press dismisses
    if (state_ == State::ERROR) {
        if (command == RemoteCommand::ENTER || command == RemoteCommand::BACK) {
            dismissScreen();
            return;
        }
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
    log_d("KindleReadableScreen state change: %d -> %d", static_cast<int>(state_),
          static_cast<int>(newState));
    state_ = newState;

    if (newState == State::CONNECTING) {
        // Start connection process
        setLoadingMessage("Connecting to network...");

        // Launch the connection in the background
        cancelFlag_ = std::make_shared<CancelFlag>();

        DispatchGetCore0BackgroundQueue()->dispatch([this]() {
            std::string errorMessage;
            bool connected = connectToNetwork(false, errorMessage);

            if (!connected) {
                DispatchGetUiQueue()->dispatch([this, errorMessage]() {
                    // Show error in UI
                    if (dismissed_) {
                        return;
                    }

                    errorMessageView_.setText(errorMessage);
                    setState(State::ERROR);
                    setNeedsRerender();
                });
                return;
            }

            // Successfully connected, now load the book content
            DispatchGetUiQueue()->dispatch([this]() {
                if (dismissed_) {
                    return;
                }

                setState(State::LOADING_CONTENT);
                setLoadingMessage("Loading Kindle book...");
                setNeedsRerender();

                // Now load the content in the background queue
                DispatchGetCore0BackgroundQueue()->dispatch([this]() { loadKindleBookPage(); });
            });
        });
    }
}

void KindleReadableScreen::setLoadingMessage(const std::string &message) {
    if (dismissed_) {
        return;
    }

    loadingMessageView_.setText(message);
    setNeedsRerender();
}

auto KindleReadableScreen::connectToNetwork(bool useBleOnly, std::string &errorMessage) -> bool {
    bool wifiConnected = false;
    bool bleConnected = false;

    SindarinAPI::getInstance()->setAccessToken(prefs_.cloudToken.get().value());

    // Try WiFi connection if not in BLE-only mode
    if (!useBleOnly && !prefs_.wifiDisabled.get().value_or(false)) {
        log_i("Attempting WiFi connection for Kindle book...");

        DispatchGetUiQueue()->dispatch([this]() {
            if (!dismissed_) {
                setLoadingMessage("Connecting to WiFi...");
            }
        });

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
    } else {
        log_i("WiFi is disabled in preferences, skipping WiFi connection");
    }

#if CONFIG_USE_BLE_SERVER_PROXY
    // Try BLE if WiFi failed or in BLE-only mode
    if ((!wifiConnected || useBleOnly) && !prefs_.bleDisabled.get().value_or(false)) {
        log_i("Attempting BLE connection for Kindle book...");

        // Disconnect WiFi if we're using BLE
        if (WiFi::getInstance().connected()) {
            log_i("Disconnecting WiFi since BLE is connecting");
            WiFi::getInstance().disconnectWiFi(3000);
        }

        DispatchGetUiQueue()->dispatch([this]() {
            if (!dismissed_) {
                setLoadingMessage("Connecting via Bluetooth...");
            }
        });

        if (!BLEManager::getInstance()->connectProxy(cancelFlag_)) {
            log_w("BLE connection failed for Kindle book");
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "kindle-ble-failed");
        } else {
            log_i("BLE successfully connected for Kindle book");
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "kindle-ble-connected");
            bleConnected = true;
        }
    }
#endif

    if (!wifiConnected && !bleConnected) {
        log_w("Failed to establish any network connection for Kindle book");
        if (prefs_.wifiDisabled.get().value_or(false)) {
            errorMessage = "WiFi is disabled in settings. Please enable WiFi to use Kindle books.";
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("kindle-books")},
                                      {"action", std::string("kindle-connection-failed")},
                                      {"reason", std::string("wifi-disabled")}});
        } else if (prefs_.bleDisabled.get().value_or(false)) {
            errorMessage = "Both WiFi and Bluetooth are disabled. Please enable one of them to "
                           "use Kindle books.";
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("kindle-books")},
                                      {"action", std::string("kindle-connection-failed")},
                                      {"reason", std::string("ble-disabled")}});
        } else {
            errorMessage =
                "Could not connect to WiFi. Please check your WiFi settings and try again.";
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("kindle-books")},
                                      {"action", std::string("kindle-connection-failed")},
                                      {"reason", std::string("wifi-connection-failed")}});
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

    // Create a response object
    auto response = std::make_shared<KindlePageResponse>();

    // For initial load, always open the book with its title - this API call runs in background
    // thread
    int httpCode = sindarinApi->openKindleBook(kindleBook_->title, response.get(), cancelFlag_);

    if (httpCode != 200 || !response->success) {
        // Handle error on the UI thread
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) {
                return;
            }

            errorMessageView_.setText("Failed to load Kindle book content");
            setState(State::ERROR);
            setNeedsRerender();
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

        // Update current page information
        currentPage_ = response->progress.currentPage;
        totalPages_ = response->progress.totalPages;
        currentPageText_ = response->ocrText;
        currentPageImageUrl_ = response->screenshotUrl;

        // Update the book progress in the KindleBook object
        kindleBook_->progressPercent = response->progress.percentage;
        kindleBook_->progress = StringFormat("%d%%", response->progress.percentage);

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

        // Update flag for future renders
        firstKindleLoad_ = false;

        // Proactively check if we need to start loading the next page in the background
        if (checkNearEndOfBuffer()) {
            log_i("Near buffer end after initial load, proactively loading more content");
            loadNextKindlePage();
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

        // Make the request
        auto sindarinApi = SindarinAPI::getInstance();
        int httpCode = sindarinApi->navigateKindleNext(response.get(), cancelFlag_);

        if (httpCode != 200 || !response->success) {
            DispatchGetUiQueue()->dispatch([this, isBackgroundLoading]() {
                if (dismissed_) {
                    return;
                }

                nextPageLoading_ = false;

                if (isBackgroundLoading) {
                    // For background loads, just log the error and restore state
                    log_w("Background loading of next Kindle page failed");
                    setState(State::DISPLAY_CONTENT);
                } else {
                    // For foreground loads, show the error
                    errorMessageView_.setText("Failed to load next page");
                    setState(State::ERROR);
                    setNeedsRerender();
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

            log_i("Processing Kindle navigation next response (success=%d, text length=%zu)",
                  response->success, response->ocrText.length());

            // Update current page information
            currentPage_ = response->progress.currentPage;
            totalPages_ = response->progress.totalPages;

            // Append the new text to the existing buffer
            if (!currentPageText_.empty()) {
                currentPageText_ += "\n\n"; // Add some spacing between pages
            }
            currentPageText_ += response->ocrText;
            currentPageImageUrl_ = response->screenshotUrl;

            // Update the book progress in the KindleBook object
            kindleBook_->progressPercent = response->progress.percentage;
            kindleBook_->progress = StringFormat("%d%%", response->progress.percentage);

            log_i("API RESPONSE - Next page: %d/%d (%d%%), new text size: %zu bytes, total buffer "
                  "size now: %zu bytes",
                  currentPage_, totalPages_, response->progress.percentage,
                  response->ocrText.length(), currentPageText_.length());

            // Create a stream with the text content
            auto stream = std::make_shared<StdStringStream>(currentPageText_, DocType::TXT);
            renderer_.setStream(stream);

            if (isBackgroundLoading) {
                // For background loading, update the stream and restore state
                log_i("Background loading complete, updated text buffer");
                setState(State::DISPLAY_CONTENT);
                
                // Get current offset and check if the current page is incomplete
                uint32_t currentOffset = currentPlace_ ? currentPlace_->getOffset() : 0;
                uint32_t totalSize = renderer_.size();
                uint32_t remainingBytes = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;
                
                bool shouldRefreshPage = false;
                
                // Only refresh the page if:
                // 1. We were previously showing an incomplete page (near end of buffer)
                // 2. The page size is less than what's typically rendered (meaning it was cut off)
                // 3. We have more content available now
                if (remainingBytes > 0 && lastRenderedPageSize_ > 0) {
                    // If the last page was smaller than typical pages (potentially cut off at buffer end)
                    // AND we now have more content, refresh to show the full page
                    if (remainingBytes < bufferThreshold_ * 2) {
                        log_i("Refreshing page after background load - current page may be incomplete (remaining: %" PRIu32 ", lastPageSize: %" PRIu32 ")", 
                              remainingBytes, lastRenderedPageSize_);
                        
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
                    }
                }
                
                // Check if we're still low on content and need to load more
                if (!shouldRefreshPage && checkNearEndOfBuffer()) {
                    log_i("Still near buffer end after background load, loading more content");
                    loadNextKindlePage();
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
                    log_i("Near buffer end after load, proactively loading more content");
                    loadNextKindlePage();
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

    // Get current buffer info
    uint32_t currentOffset = 0;
    if (currentPlace_) {
        currentOffset = currentPlace_->getOffset();
    }

    uint32_t totalSize = renderer_.size();
    uint32_t remainingBytes = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;

    // For Kindle books, we need special handling for EOF
    // Check if we're physically near the end of the buffer
    bool isAtEndOfBuffer =
        (remainingBytes < bufferThreshold_) ||
        (renderer_.atEndOfFile() && totalSize > 0 &&
         remainingBytes < bufferThreshold_ * 2 // Be more conservative for user interactions
        );

    // Check if we're at the end of the buffer and waiting for more content
    if (isAtEndOfBuffer) {
        log_i("End of buffer detected - remaining bytes: %" PRIu32 ", buffer size: %" PRIu32 "", remainingBytes,
              totalSize);

        if (state_ == State::LOADING_BACKGROUND) {
            // We're already loading more content, show a temporary message
            setLoadingMessage("Loading more content...");
            // Don't change state, just show the message
            setNeedsRerender();
            return;
        } else if (nextPageLoading_) {
            // We're loading in background but not tracked in state yet
            // Start showing loading UI since user has reached the end
            setLoadingMessage("Loading more content...");
            setState(State::LOADING_BACKGROUND);
            setNeedsRerender();
            return;
        } else {
            // We've reached the end but aren't loading more - start loading
            loadNextKindlePage();
            return;
        }
    }

    // For proper navigation, we should use the next place that was returned
    // from the previous render operation, which represents exactly how much
    // text was rendered on the current screen

    // We've already got currentOffset and totalSize from above,
    // no need to redefine them

    // If newPlace_ is null or somehow invalid, create a reasonable fallback
    uint32_t nextOffset = currentOffset;
    if (newPlace_) {
        nextOffset = newPlace_->getOffset();
    } else {
        // If we don't have a valid next place, this is a fallback
        log_w("No valid next place found, using fallback navigation");
        nextOffset = currentOffset + 500; // Fallback: approximate screen of text
    }

    // Check if we'd go past the end of the buffer
    if (totalSize > 0 && nextOffset >= totalSize) {
        // We'd go past the end, so we need more content
        log_i("Next offset %" PRIu32 " would go past buffer size %" PRIu32 "", nextOffset, totalSize);
        if (!nextPageLoading_) {
            loadNextKindlePage();
        }
        return;
    }

    // Log navigation info with buffer details
    uint32_t remainingBeforeNav = (currentOffset < totalSize) ? (totalSize - currentOffset) : 0;
    uint32_t remainingAfterNav = (nextOffset < totalSize) ? (totalSize - nextOffset) : 0;
    uint32_t pageSize = nextOffset - currentOffset;

    log_i("NAVIGATION - Moving from offset %" PRIu32 " to %" PRIu32 " (page size: %" PRIu32 " bytes), buffer size: %" PRIu32 ", "
          "remaining before: %" PRIu32 ", after: %" PRIu32 " bytes",
          currentOffset, nextOffset, pageSize, totalSize, remainingBeforeNav, remainingAfterNav);

    // Create a new place if needed
    if (!newPlace_) {
        newPlace_ = Place::factory(nextOffset);
    }

    // Turn to the next page in the renderer buffer
    turnPage();

    // Render the content from the new position
    uint8_t darkPicturePercent = 0;
    std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(currentPlace_);

    // Update the display
    display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                           display_.darkPictureVComAdj(darkPicturePercent));

    log_i("Navigated to next page within current buffer");

    // Check if we need to load more content preemptively
    if (checkNearEndOfBuffer() && !nextPageLoading_) {
        log_i("Preemptively loading next content after navigation");
        loadNextKindlePage();
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
        int httpCode = sindarinApi->navigateKindlePrevious(response.get(), cancelFlag_);

        if (httpCode != 200 || !response->success) {
            DispatchGetUiQueue()->dispatch([this]() {
                if (dismissed_) {
                    return;
                }

                errorMessageView_.setText("Failed to load previous page");
                setState(State::ERROR);
                setNeedsRerender();
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
        loadPreviousKindlePage();
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

        log_w("No valid previous place found, using fallback navigation offset %" PRIu32 " (pageSize: %" PRIu32 ")",
              prevOffset, pageSize);

        prevPlace = Place::factory(prevOffset);
    }

    // Get the offset for logging
    uint32_t prevOffset = prevPlace->getOffset();
    uint32_t totalSize = renderer_.size();
    uint32_t pageSize = currentOffset - prevOffset;

    log_i(
        "NAVIGATION - Moving backward from offset %" PRIu32 " to %" PRIu32 " (page size: %" PRIu32 " bytes), buffer size: %" PRIu32,
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
    float percentThrough = (totalSize > 0) ? (currentOffset * 100.0f / totalSize) : 0;

    // Check if we're close to the end of the buffer
    if (totalSize > 0 && remainingBytes < bufferThreshold_) {
        log_i("BUFFER CHECK - Near end: offset=%" PRIu32 "/%" PRIu32 " (%.1f%%), remaining=%" PRIu32 " bytes, threshold=%d "
              "bytes",
              currentOffset, totalSize, percentThrough, remainingBytes, bufferThreshold_);
        return true;
    }

    // For Kindle books, be careful about the EOF detection
    if (renderer_.atEndOfFile()) {
        // We're loading content dynamically, so check if we're
        // near the physical end of the buffer instead of relying on EOF flag
        if (remainingBytes < bufferThreshold_) {
            log_i("BUFFER CHECK - Treating EOF as 'near end' at offset %" PRIu32 "/%" PRIu32 " (%.1f%%)",
                  currentOffset, totalSize, percentThrough);
            return true;
        } else {
            // If we have enough buffer space left, don't trigger loading yet
            log_i("BUFFER CHECK - Ignoring EOF flag because buffer still has %" PRIu32 " bytes",
                  remainingBytes);
            return false;
        }
    }

    // Not near end, log status for debugging
    log_i("BUFFER CHECK - Not near end: offset=%" PRIu32 "/%" PRIu32 " (%.1f%%), remaining=%" PRIu32 " bytes, threshold=%d "
          "bytes",
          currentOffset, totalSize, percentThrough, remainingBytes, bufferThreshold_);

    return false;
}

void KindleReadableScreen::turnPage() {
    if (atEnd_) {
        // If at end, don't advance (similar to ReadableScreen)
        log_i("At the end of content, not advancing page");
        return;
    }

    // Save previous offset for logging
    uint32_t prevOffset = 0;
    if (currentPlace_) {
        prevOffset = currentPlace_->getOffset();
    }

    // Save current place
    if (newPlace_) {
        // Update the current place with the new place
        currentPlace_ = newPlace_;
        // Set the renderer's current place to the new place
        renderer_.setCurrentPlace(currentPlace_);

        // Get buffer info for logging
        uint32_t newOffset = currentPlace_->getOffset();
        uint32_t totalSize = renderer_.size();
        uint32_t remainingBytes = (newOffset < totalSize) ? (totalSize - newOffset) : 0;
        float percentThrough = (totalSize > 0) ? (newOffset * 100.0f / totalSize) : 0;

        // Calculate and store the page size
        lastRenderedPageSize_ = (newOffset > prevOffset) ? (newOffset - prevOffset) : 0;
              
        // Log the page turn with detailed buffer information
        log_i("PAGE TURN - Updated place from offset %" PRIu32 " to %" PRIu32 " (%.1f%% of buffer), remaining: %" PRIu32 " "
              "bytes, page size: %" PRIu32 " bytes",
              prevOffset, newOffset, percentThrough, remainingBytes, lastRenderedPageSize_);
    } else {
        // This shouldn't happen, but handle it gracefully
        currentPlace_ = renderer_.getCurrentPlace();
        log_i("turnPage - NO NEW PLACE, using current place offset: %" PRIu32,
              (uint32_t)(currentPlace_ ? currentPlace_->getOffset() : 0));
    }

    // Check if we're at the end
    atEnd_ = renderer_.atEndOfFile();

    // For debug
    log_i("turnPage - at end of content: %s", atEnd_ ? "yes" : "no");
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
    // Disconnect BLE if connected
    if (BLEManager::getInstance()) {
        BLEManager::getInstance()->disconnectProxy();
    }
#endif
}

void KindleReadableScreen::dismissScreen() {
    if (dismissed_) {
        return;
    }

    dismissed_ = true;

    // Disconnect network connections
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

    // Reset renderer
    renderer_.setStream(nullptr);

    // Trigger a full display update
    display_.updateDisplay(DisplaySystem::UpdateType::FULL, true);
}

auto KindleReadableScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    // For non-content states, center the message views
    if (state_ != State::DISPLAY_CONTENT) {
        const auto inset = 2;
        auto contentFrame = SolRect::fromSize(size).inset(inset);

        // Center the message views vertically and horizontally
        context.setViewFrame(loadingMessageView_, contentFrame,
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                                 LayoutOptionsAlignCenterVertical);
        context.setViewFrame(errorMessageView_, contentFrame,
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                                 LayoutOptionsAlignCenterVertical);
    }

    return size;
}

void KindleReadableScreen::drawInRect(SolRect rect) {
    // For non-content states, draw the appropriate message view
    if (state_ != State::DISPLAY_CONTENT) {
        switch (state_) {
        case State::CONNECTING:
        case State::LOADING_CONTENT:
            loadingMessageView_.draw();
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
    
    readablePlaceholder->place = currentPlace_;  // Use current reading position
    
    // Show the popup menu - no need to set readable since that's only needed for some features
    setCurrentScreen(popupMenuScreen_);
    setNeedsRerender();
}
