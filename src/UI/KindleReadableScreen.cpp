#include "UI/KindleReadableScreen.hpp"

#include <esp_timer.h>

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEManager.hpp"
#endif
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "Menu/ReadingSettingsMenuScreen.hpp"
#include "Models/Readable.hpp"
#include "UI/Fonts/Fonts.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "sindarin-debug.h"

KindleReadableScreen::KindleReadableScreen(DisplaySystem &display,
                                           const std::shared_ptr<KindleBook> &kindleBook,
                                           PreferencesStore &prefs, Telemetry &telemetry,
                                           FileSystem &fileSystem,
                                           LightingController &lightingController,
                                           LocalStore &localStore, Font *font)
    : EinkView(display), renderer_(display, GetTextRectFromPrefs(display, prefs), font),
      prefs_(prefs), telemetry_(telemetry), fileSystem_(fileSystem),
      lightingController_(lightingController), localStore_(localStore), kindleBook_(kindleBook),
      cancelFlag_(std::make_shared<CancelFlag>()),
      pageFlipper_(kindleBook, cancelFlag_, telemetry_), loadingMessageView_(display, 5),
      delayedMessageView_(display, 5), errorMessageView_(display, 5) {

    log_i("Created KindleReadableScreen for book: %s by %s", kindleBook_->title.c_str(),
          kindleBook_->author.c_str());

    currentPlace_ = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));

    updatePageFlipperCallbacks();

    renderer_.setStream(pageFlipper_.getStream());
    renderer_.setLineSpacing(prefs_.lineSpacing.get().value_or(0));

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

    // Set up delayed message view (for "This can take up to 60 seconds")
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

        menuItems->emplace_back(
            std::make_shared<MenuItemSingleLine>("Main Menu", [this]() { dismissScreen(); }));

        menuItems->emplace_back(std::make_shared<MenuItemSingleLine>("Brightness", [this]() {
            auto brightness = std::make_shared<LightingAdjustScreen>(
                display_, prefs_, lightingController_, telemetry_);

            brightness->setBackCallback([this]() {
                setCurrentScreen(nullptr);
                setNeedsRerender();
            });
            setCurrentScreen(brightness);
        }));

        menuItems->emplace_back(std::make_shared<MenuItemSingleLine>("Back to Text", [this]() {
            display_.clearDisplay();
            setCurrentScreen(nullptr);
            setNeedsRerender();
        }));

        return menuItems;
    };

    auto popupStyleFactory = [this]() -> MenuItemSharedPtrVector {
        auto menuItems = std::make_shared<MenuItemVector>();

        auto menuItem = std::make_shared<MenuItemSingleLine>("Text Size", [this]() {
            auto fontMenu = std::make_shared<ReadingSettingsMenuScreen>(
                display_, "Back to Text", prefs_, telemetry_,
                [this](const std::shared_ptr<Screen> &screen) { setCurrentScreen(screen); },
                [this]() { setCurrentScreen(nullptr); });
            fontMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Small", [this]() {
                display_.clearDisplay();
                prefs_.font.set(PREFS_SOL_SMALL);
                telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                         {{"text_size", std::string("small")}});
                getCurrentScreen()->setNeedsRerender();
            }));
            fontMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Medium", [this]() {
                display_.clearDisplay();
                prefs_.font.set(PREFS_SOL_MEDIUM);
                telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                         {{"text_size", std::string("medium")}});
                getCurrentScreen()->setNeedsRerender();
            }));
            fontMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Large", [this]() {
                display_.clearDisplay();
                prefs_.font.set(PREFS_SOL_LARGE);
                telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                         {{"text_size", std::string("large")}});
                getCurrentScreen()->setNeedsRerender();
            }));
            int menuIndex = 0;
            if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_SMALL) {
                menuIndex = 0;
            } else if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_MEDIUM) {
                menuIndex = 1;
            } else if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_LARGE) {
                menuIndex = 2;
            }
            fontMenu->setSelectedIndex(menuIndex);

            setCurrentScreen(fontMenu);
            fontMenu->setBackCallback([this]() {
                Font *font = FontFromPrefs(prefs_.font.get().value_or(PREFS_SOL_SMALL));
                if (font) {
                    renderer_.setFont(font);
                }
                setCurrentScreen(nullptr);
            });
        });

        menuItems->emplace_back(menuItem);

        menuItem = std::make_shared<MenuItemSingleLine>("Move Text", [this]() {
            log_d("Loading adjust offset");
            auto offsetAdjust = std::make_shared<OffsetAdjustScreen>(display_, prefs_, telemetry_,
                                                                     renderer_, currentPlace_);
            setCurrentScreen(offsetAdjust);
            log_d("setting up callback");
            offsetAdjust->setBackCallback([this]() {
                renderer_.setRect(GetTextRectFromPrefs(display_, prefs_));
                setCurrentScreen(nullptr);
            });
            log_d("set up callback");
        });

        menuItems->emplace_back(menuItem);

        menuItem = std::make_shared<MenuItemSingleLine>("Resize Page", [this]() {
            log_d("Loading resize box");
            auto resizeBox = std::make_shared<ResizeBoxScreen>(display_, prefs_, telemetry_,
                                                               renderer_, currentPlace_);
            setCurrentScreen(resizeBox);
            log_d("setting up callback");
            resizeBox->setBackCallback([this]() {
                renderer_.setRect(GetTextRectFromPrefs(display_, prefs_));
                setCurrentScreen(nullptr);
            });
            log_d("set up callback");
        });

        menuItems->emplace_back(menuItem);

        menuItem = std::make_shared<MenuItemSingleLine>("Line Spacing", [this]() {
            auto lineSpacingScreen = std::make_shared<LineSpacingScreen>(
                display_, prefs_, telemetry_, renderer_, currentPlace_);
            setCurrentScreen(lineSpacingScreen);
            lineSpacingScreen->setBackCallback([this]() {
                setCurrentScreen(nullptr);
                // Update the renderer with the new line spacing -- needed here, otherwise we'd have
                // to do it in the render() method and that'd be reading from NVS a lot...
                renderer_.setLineSpacing(prefs_.lineSpacing.get().value_or(0));
            });
        });

        menuItems->emplace_back(menuItem);

        auto menuItemCheckable = std::make_shared<MenuItemCheckable>(
            "Dark Mode",
            [this]() {
            prefs_.darkMode.set(!prefs_.darkMode.get().value_or(false));
            // Reserved for future use? Potentially lighting up and down?
            display_.flipInversion();
            // When we invert, we should do a full clear of the display to make sure
            // the blacks are really gone.
            display_.clearDisplay();
            prefs_.invertSetting.set(display_.getInversion());
            telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                     {{"dark_mode", prefs_.darkMode.get().value_or(false)}});
            display_.clearDisplay();
            setNeedsRerender();
            setFirstLoad();
            },
            [this]() -> bool { return prefs_.darkMode.get().value_or(false); });

        menuItems->emplace_back(menuItemCheckable);

        menuItem = std::make_shared<MenuItemSingleLine>("Back to Text", [this]() {
            display_.clearDisplay();
            setCurrentScreen(nullptr);
            setNeedsRerender();
        });

        menuItems->emplace_back(menuItem);

        return menuItems;
    };

    // Create a readable-like object for the popup menu to use
    // TODO(john): We should make a KindleReadable object that can be used for Kindle books
    auto readablePlaceholder = std::make_shared<Readable>();
    readablePlaceholder->title = kindleBook_->title;

    // Add authors from kindle book to readable
    Author author;
    author.name = kindleBook_->author;
    readablePlaceholder->authors.push_back(author);

    auto stream = pageFlipper_.getStream();
    popupMenuScreen_ = std::make_shared<NewReadablePopupMenuScreen>(
        display, prefs_, fileSystem_, readablePlaceholder, lightingController_, renderer_,
        telemetry_, localStore_, stream);

    popupMenuScreen_->addTab("Popup", popupNavigationFactory, 8);
    popupMenuScreen_->addTab("Style", popupStyleFactory, 8);
    popupMenuScreen_->populateMenuView();
}

void KindleReadableScreen::begin() {
    // Reset reconnection attempts counter
    reconnectionAttempts_ = 0;

    // Log telemetry for opening a Kindle book
    auto metrics = getTelemetryMetrics();
    metrics["kindle"] = true;
    telemetry_.logMetric(TelemetryField::METRIC_READABLE_OPEN, kindleBook_->uuid, metrics);

    // Capture cancel flag before the callback to avoid use-after-free
    auto cancelFlag = cancelFlag_;

    // Define the callback for initial connection - load the book content
    auto initialConnectionCallback = [this, cancelFlag](bool connected) {
        if (!connected) {
            // Connection failed - error message is handled by connectToNetwork
            return;
        }

        // Successfully connected, now load the book content
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }

        // Record book open start time (content loading phase)
        bookOpenStartTime_ = esp_timer_get_time();

        DispatchGetUiQueue()->dispatch([this, cancelFlag]() {
            if (CancelFlag::isCancelled(cancelFlag)) {
                return;
            }

            log_d("Setting loading message to \"Opening your Kindle book...\"");

            loadingMessageView_.setText("Opening your Kindle book...");
            setNeedsLayout();
            setState(State::LOADING_INITIAL_CONTENT);
            render(display_);

            // Schedule a delayed message after 30 seconds if loading takes too long
            scheduleDelayedLoadingMessage();
        });

        // Now load the content in the background queue
        pageFlipper_.loadInitialPage();
    };

    // Start connection process
    DispatchGetCore0BackgroundQueue()->dispatch([this, initialConnectionCallback, cancelFlag]() {
        std::string errorMessage;
        // Record connection start time for kindle-connect elapsed_ms tracking
        connectionStartTime_ = esp_timer_get_time();
        bool connected = connectToNetwork(errorMessage, initialConnectionCallback);

        // Show error if initial connection failed completely
        if (!connected && !errorMessage.empty()) {
            DispatchGetUiQueue()->dispatch([this, errorMessage, cancelFlag]() {
                if (CancelFlag::isCancelled(cancelFlag)) {
                    return;
                }
                setErrorMessage(errorMessage);
            });
        }
    });
}

auto KindleReadableScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    context.setViewFrame(errorMessageView_, SolRect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    context.setViewFrame(loadingMessageView_, SolRect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    context.setViewFrame(delayedMessageView_, SolRect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);

    return size;
}

void KindleReadableScreen::drawInRect(SolRect rect) {
    loadingMessageView_.draw();
    errorMessageView_.draw();
    delayedMessageView_.draw();

    if (state_ == State::DISPLAY_CONTENT || state_ == State::DISPLAY_PARTIAL_CONTENT) {
        // Render the current page content
        uint8_t darkPicturePercent = 0;

        // Render using our current place and get the next place
        std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(currentPlace_);

        // If we're at the end of loaded content
        // TODO(john): Differentiate between end of loaded content and end of book
        bool atEndOfContent = renderer_.atEndOfFile();
        if (atEndOfContent) {
            // Log telemetry for showing partial content
            telemetry_.logMetric(
                TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                {{"event", std::string("page-turn")},
                 {"action", std::string("partial-content-shown")},
                 {"kindle", true},
                 {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0}});

            setState(State::DISPLAY_PARTIAL_CONTENT);
        }

        // Log comprehensive buffer state for debugging
        uint32_t currentOffset = currentPlace_->getOffset();
        uint32_t nextOffset = newPlace_->getOffset();
        uint32_t totalSize = renderer_.size();
        uint32_t bytesRemaining = totalSize > nextOffset ? totalSize - nextOffset : 0;
        uint32_t bytesRenderedThisPage = nextOffset - currentOffset;

        log_d("Buffer state: current offset: %" PRIu32 ", next offset: %" PRIu32
              ", total buffered: %" PRIu32 " bytes",
              currentOffset, nextOffset, totalSize);
        log_d("Page rendered %" PRIu32 " bytes, %" PRIu32 " bytes remaining in buffer",
              bytesRenderedThisPage, bytesRemaining);
    }
}

void KindleReadableScreen::render(DisplaySystem &display) {
    // If we have a popup menu or other screen, let it handle rendering
    if (currentScreen_) {
        log_d("Letting current screen (a popup menu) render KindleReadableScreen, state: %d",
              static_cast<int>(state_));
        currentScreen_->render(display);
        return;
    }
    log_d("Rendering KindleReadableScreen, state: %d", static_cast<int>(state_));

    setFrame(defaultFrameRect());
    draw();

    display.updateDisplay(updateType_, true);
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

    // Handle navigation commands specially for Kindle content
    switch (command) {
    case RemoteCommand::RIGHT:
    case RemoteCommand::ENTER:
        if (state_ == State::DISPLAY_CONTENT || state_ == State::DISPLAY_PARTIAL_CONTENT) {
            navigateNext();
        }
        break;
    case RemoteCommand::LEFT:
        if (state_ == State::DISPLAY_CONTENT || state_ == State::DISPLAY_PARTIAL_CONTENT) {
            navigatePrevious();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::UP:
    case RemoteCommand::DOWN:
        showPopupMenu();
        break;
    case RemoteCommand::ANIMATION:
    default:
        break;
    }
}

auto KindleReadableScreen::stateToString(State state) -> const char * {
    switch (state) {
    case State::CONNECTING:
        return "CONNECTING";
    case State::LOADING_INITIAL_CONTENT:
        return "LOADING_INITIAL_CONTENT";
    case State::LOADING_CONTENT:
        return "LOADING_CONTENT";
    case State::ERROR:
        return "ERROR";
    case State::DISPLAY_CONTENT:
        return "DISPLAY_CONTENT";
    case State::SHOWING_DIALOG:
        return "SHOWING_DIALOG";
    case State::DISPLAY_PARTIAL_CONTENT:
        return "DISPLAY_PARTIAL_CONTENT";
    default:
        return "UNKNOWN";
    }
}

void KindleReadableScreen::setState(State newState) {
    updateType_ = DisplaySystem::UpdateType::FULL;

    switch (newState) {
    case State::CONNECTING:
    case State::LOADING_INITIAL_CONTENT:
    case State::LOADING_CONTENT:
        loadingMessageView_.setHidden(false);
        delayedMessageView_.setHidden(true);
        errorMessageView_.setHidden(true);
        break;
    case State::ERROR:
        loadingMessageView_.setHidden(true);
        delayedMessageView_.setHidden(true);
        errorMessageView_.setHidden(false);
        break;
    case State::DISPLAY_CONTENT:
    case State::DISPLAY_PARTIAL_CONTENT:
        loadingMessageView_.setHidden(true);
        delayedMessageView_.setHidden(true);
        errorMessageView_.setHidden(true);
        // When going from partial content to full page content, always only do a partial
        // refresh
        if (state_ == State::DISPLAY_PARTIAL_CONTENT) {
            updateType_ = DisplaySystem::UpdateType::PARTIAL;
        }
        break;
    case State::SHOWING_DIALOG:
        loadingMessageView_.setHidden(true);
        delayedMessageView_.setHidden(true);
        errorMessageView_.setHidden(true);
        break;
    }

    log_d("KindleReadableScreen state change: %s -> %s", stateToString(state_),
          stateToString(newState));
    state_ = newState;
    firstLoad_ = true;
}

void KindleReadableScreen::setErrorMessage(const std::string &message) {
    if (CancelFlag::isCancelled(cancelFlag_)) {
        return;
    }
    log_i("Showing error message: %s", message.c_str());

    telemetry_.logMetric(
        TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
        {{"event", std::string("book-error")},
         {"action", std::string("error")},
         {"kindle", true},
         {"error", message},
         {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0}});

    errorMessageView_.setText(message);
    setState(State::ERROR);
    setNeedsLayout();
    setNeedsRerender();
    render(display_);

    // TODO: Re-add auto-dismiss?
}

auto KindleReadableScreen::connectToNetwork(std::string &errorMessage,
                                            const ConnectionCallback &callback, int bleTimeoutMs)
    -> bool {
    log_d("Connecting to network for Kindle book");
    bool wifiConnected = false;
    bool bleConnected = false;

    // Reset connection tracking
    isUsingBleConnection_ = false;

    SindarinAPI::getInstance()->setAccessToken(prefs_.cloudToken.get().value());

    // Determine which connection method we'll try
    std::string connectionMethod;
    bool bleDisabled = prefs_.bleDisabled.get().value_or(false);
    bool wifiDisabled = prefs_.wifiDisabled.get().value_or(false);

#if CONFIG_USE_BLE_SERVER_PROXY
    if (!bleDisabled) {
        connectionMethod = "ble";
    } else if (!wifiDisabled) {
        connectionMethod = "wifi";
    } else {
        connectionMethod = "none";
    }
#else
    if (!wifiDisabled) {
        connectionMethod = "wifi";
    } else {
        connectionMethod = "none";
    }
#endif

    // Log single comprehensive connection attempt telemetry
    std::string attemptAction = "attempt-" + connectionMethod;
    int64_t initialElapsedMs = (esp_timer_get_time() - connectionStartTime_) / 1000;
    telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                         {{"event", std::string("kindle-connect")},
                          {"action", attemptAction},
                          {"kindle", true},
                          {"wifi_disabled", wifiDisabled},
                          {"ble_disabled", bleDisabled},
                          {"state", std::string(stateToString(state_))},
                          {"elapsed_ms", static_cast<int>(initialElapsedMs)}});

#if CONFIG_USE_BLE_SERVER_PROXY
    // Try BLE first if not disabled
    if (!bleDisabled) {
        log_i("Attempting BLE connection for Kindle book...");

        // Disconnect WiFi if we're using BLE
        if (WiFi::getInstance().connected()) {
            log_i("Disconnecting WiFi since BLE is connecting");
            WiFi::getInstance().disconnectWiFi(3000);
        }

        // For reconnection, first disconnect any stale connection
        auto bleManager = BLEManager::getInstance();
        if (bleManager->connected()) {
            log_i("Disconnecting existing BLE connection first");
            bleManager->disconnectProxy();

            // Give it a moment to disconnect
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        if (!bleManager->connectProxy(cancelFlag_, bleTimeoutMs)) {
            log_w("BLE connection failed for Kindle book");

            // Log BLE failure telemetry
            int64_t bleElapsedMs = (esp_timer_get_time() - connectionStartTime_) / 1000;
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 {{"event", std::string("kindle-connect")},
                                  {"action", std::string("failed-ble")},
                                  {"kindle", true},
                                  {"state", std::string(stateToString(state_))},
                                  {"elapsed_ms", static_cast<int>(bleElapsedMs)}});
        } else {
            log_i("BLE successfully connected for Kindle book");
            bleConnected = true;
            isUsingBleConnection_ = true;

            // Register for BLE disconnection events
            auto localCancelFlag = cancelFlag_;
            bleManager->setDisconnectionCallback([this, localCancelFlag]() {
                if (CancelFlag::isCancelled(localCancelFlag)) {
                    return;
                }
                log_i("BLE disconnected, canceling ongoing operations and initiating reconnection");

                // Cancel any pending BLE requests and create new cancel flag
                if (cancelFlag_) {
                    CancelFlag::cancel(cancelFlag_);
                }
                cancelFlag_ = std::make_shared<CancelFlag>();
                pageFlipper_.setCancelFlag(cancelFlag_);

                // Re-register callbacks with the new cancel flag
                updatePageFlipperCallbacks();

                // Dispatch reconnection attempt - don't check the old cancel flag here
                // since we just cancelled it
                DispatchGetUiQueue()->dispatch([this]() {
                    if (!isReconnecting_) {
                        attemptReconnection();
                    } else {
                        log_i("BLE disconnected during reconnection attempt, ignoring");
                    }
                });
            });
        }
    }
#endif

    // Try WiFi if BLE failed
    if (!bleConnected && !wifiDisabled) {
        log_i("Attempting WiFi connection for Kindle book...");

        // Log WiFi attempt telemetry
        int64_t wifiAttemptElapsedMs = (esp_timer_get_time() - connectionStartTime_) / 1000;
        telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                             {{"event", std::string("kindle-connect")},
                              {"action", std::string("attempt-wifi")},
                              {"kindle", true},
                              {"wifi_disabled", wifiDisabled},
                              {"ble_disabled", bleDisabled},
                              {"state", std::string(stateToString(state_))},
                              {"elapsed_ms", static_cast<int>(wifiAttemptElapsedMs)}});

        if (!WiFi::getInstance().connectWiFiFromSettings()) {
            log_w("WiFi connection failed for Kindle book");
            wifiConnected = false;
        } else {
            log_i("WiFi connection successful for Kindle book");
            wifiConnected = true;
            bool wasPreviouslyUsingBle = isUsingBleConnection_;
            isUsingBleConnection_ = false;
            if (wasPreviouslyUsingBle && reconnectionAttempts_ > 0) {
                log_i("Successfully switched from BLE to Wi-Fi during reconnection");
                int64_t fallbackElapsedMs = (esp_timer_get_time() - connectionStartTime_) / 1000;
                telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                     {{"event", std::string("kindle-connect")},
                                      {"action", std::string("ble-to-wifi-fallback")},
                                      {"kindle", true},
                                      {"state", std::string(stateToString(state_))},
                                      {"elapsed_ms", static_cast<int>(fallbackElapsedMs)}});
            }
        }
    } else {
        log_i("WiFi is disabled in preferences, skipping WiFi connection");
    }

    bool connected = wifiConnected || bleConnected;

    if (!connected) {
        log_w("Failed to establish any network connection for Kindle book");
        if (wifiDisabled && bleDisabled) {
            errorMessage = "Both WiFi and Bluetooth are disabled. Please enable one of them to "
                           "use Kindle books.";
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 {{"event", std::string("book-open-failed-network")},
                                  {"action", std::string("both-wifi-ble-disabled")},
                                  {"kindle", true}});
        } else if (wifiDisabled) {
            errorMessage = "WiFi is disabled in settings and BLE connection failed. Please check "
                           "your BLE connection or enable WiFi to use Kindle books.";
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 {{"event", std::string("book-open-failed-network")},
                                  {"action", std::string("wifi-disabled-ble-failed")},
                                  {"kindle", true}});
        } else if (bleDisabled) {
            errorMessage = "Bluetooth is disabled in settings and WiFi connection failed. Please "
                           "check your WiFi connection or enable Bluetooth to use Kindle books.";
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 {{"event", std::string("book-open-failed-network")},
                                  {"action", std::string("ble-disabled-wifi-failed")},
                                  {"kindle", true}});
        } else {
            errorMessage = "Could not connect to network. Please check your WiFi and Bluetooth "
                           "settings and try again.";
            telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                                 {{"event", std::string("book-open-failed-network")},
                                  {"action", std::string("connection-failed")},
                                  {"kindle", true}});
        }
    }

    // Update page flipper with the connection type
    pageFlipper_.setUseBle(isUsingBleConnection_);
    log_i("Setting KindlePageFlipper to use BLE: %s", isUsingBleConnection_ ? "true" : "false");

    std::string resultAction;
    if (connected) {
        resultAction = isUsingBleConnection_ ? "success-ble" : "success-wifi";
    } else {
        resultAction = "failed";
    }

    int64_t finalElapsedMs = (esp_timer_get_time() - connectionStartTime_) / 1000;
    telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                         {{"event", std::string("kindle-connect")},
                          {"action", resultAction},
                          {"kindle", true},
                          {"ble_attempted", connectionMethod == "ble"},
                          {"ble_connected", bleConnected},
                          {"wifi_connected", wifiConnected},
                          {"state", std::string(stateToString(state_))},
                          {"elapsed_ms", static_cast<int>(finalElapsedMs)}});

    // Call the callback if provided
    if (callback) {
        callback(connected);
    }

    return connected;
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
        // Clear the disconnection callback to avoid dangling references
        bleManager->setDisconnectionCallback(nullptr);

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

void KindleReadableScreen::navigateNext() {
    // Transition to loading state if we're showing partial content
    if (state_ == State::DISPLAY_PARTIAL_CONTENT) {
        // Record navigation start time
        navigationStartTime_ = esp_timer_get_time();

        // Log telemetry for showing loading screen during page turn
        telemetry_.logMetric(
            TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
            {{"event", std::string("page-turn")},
             {"action", std::string("loading-screen-shown")},
             {"kindle", true},
             {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0}});

        setState(State::LOADING_CONTENT);
        loadingMessageView_.setText("Loading next page...");
        setNeedsLayout();
        setNeedsRerender();

#ifdef TESTS
        pageFlipper_.notifyTestContentReady();
#endif

        return;
    }

    currentPlace_ = newPlace_;

    // Log telemetry for navigating to next page
    auto metrics = getTelemetryMetrics();
    metrics["kindle"] = true;

    // Add buffer state to telemetry
    uint32_t nextOffset = newPlace_->getOffset();
    uint32_t totalSize = renderer_.size();
    uint32_t bytesRemaining = totalSize > nextOffset ? totalSize - nextOffset : 0;
    metrics["br"] = static_cast<int>(bytesRemaining);                     // bytes remaining
    metrics["pr"] = pageFlipper_.getPagesRemainingFromOffset(nextOffset); // pages remaining

    telemetry_.logMetric(TelemetryField::METRIC_READABLE_PLACE, kindleBook_->uuid, metrics);

    updateType_ = prefs_.fastTransitions.get().value_or(false) ? DisplaySystem::UpdateType::PARTIAL
                                                               : DisplaySystem::UpdateType::FULL;
    setNeedsRerender();

    // Notify page flipper of our new position after successful local navigation
    pageFlipper_.updatePlace(currentPlace_);
}

void KindleReadableScreen::navigatePrevious() {
    currentPlace_ = renderer_.getPreviousPlace();

    // Log telemetry for navigating to previous page
    auto metrics = getTelemetryMetrics();
    metrics["kindle"] = true;

    // Add buffer state to telemetry
    uint32_t currentOffset = currentPlace_->getOffset();
    uint32_t totalSize = renderer_.size();
    uint32_t bytesRemaining = totalSize > currentOffset ? totalSize - currentOffset : 0;
    metrics["br"] = static_cast<int>(bytesRemaining);                        // bytes remaining
    metrics["pr"] = pageFlipper_.getPagesRemainingFromOffset(currentOffset); // pages remaining

    telemetry_.logMetric(TelemetryField::METRIC_READABLE_PLACE, kindleBook_->uuid, metrics);

    updateType_ = prefs_.fastTransitions.get().value_or(false) ? DisplaySystem::UpdateType::PARTIAL
                                                               : DisplaySystem::UpdateType::FULL;
    setNeedsRerender();

    // Notify page flipper of our new position after successful local navigation
    pageFlipper_.updatePlace(currentPlace_);
}

void KindleReadableScreen::dismissScreen() {
    log_d("Dismissing KindleReadableScreen");

    // Log telemetry for closing a Kindle book
    int64_t elapsedMs = (esp_timer_get_time() - bookOpenStartTime_) / 1000;
    telemetry_.logMetric(TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                         {{"event", std::string("book-close")},
                          {"action", std::string("book-closed")},
                          {"kindle", true},
                          {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0},
                          {"elapsed_ms", static_cast<int>(elapsedMs)}});

    CancelFlag::cancel(cancelFlag_);
    cancelDelayedLoadingMessage();
    cancelReconnectionAttempt();
#ifdef DEBUG_KINDLE_RECONNECTION
    cancelTestDisconnect();
#endif

    // Reset reconnection attempts counter and flags
    reconnectionAttempts_ = 0;
    isReconnecting_ = false;

    disconnectNetworkConnections();
    back();
}

void KindleReadableScreen::showPopupMenu() {
    // Create a readable-like object for the popup menu to use
    // TODO(john): Don't we already do this?
    auto readablePlaceholder = std::make_shared<Readable>();
    readablePlaceholder->title = kindleBook_->title;

    // Add authors from kindle book to readable
    Author author;
    author.name = kindleBook_->author;
    readablePlaceholder->authors.push_back(author);

    readablePlaceholder->place = currentPlace_; // Use current reading position

    // Show the popup menu - no need to set readable since that's only needed for some features
    setCurrentScreen(popupMenuScreen_);
    updateType_ = DisplaySystem::UpdateType::PARTIAL;
    setNeedsRerender();
}

void KindleReadableScreen::showLastReadDialog(const SpiramString &dialogText) {
    log_i("Showing last read dialog: %s", dialogText.c_str());

    setState(State::SHOWING_DIALOG);

    // Create the last read dialog screen
    auto lastReadDialog = std::make_shared<KindleLastReadDialogScreen>(display_, dialogText);

    // Set the callback for when the user makes a selection
    lastReadDialog->setResponseCallback(
        [this](bool gotoLastReadPage) { handleLastReadDialogResponse(gotoLastReadPage); });

    // Show the dialog
    setCurrentScreen(lastReadDialog);
    updateType_ = DisplaySystem::UpdateType::PARTIAL;
    setNeedsRerender();
    render(display_);

#ifdef TESTS
    if (onTestLastReadDialogShown_) {
        log_i("TEST: last-read dialog shown callback invoked");
        onTestLastReadDialogShown_();
    } else {
        log_w("TEST: last-read dialog shown callback not set");
    }
#endif

    // Log that we showed the last read dialog
    telemetry_.logMetric(TelemetryField::METRIC_DIALOG, kindleBook_->uuid,
                         {{"dialog", std::string("kindle-last-read")},
                          {"action", std::string("last-read-dialog-shown")},
                          {"kindle", true}});
}

void KindleReadableScreen::handleLastReadDialogResponse(bool gotoLastReadPage) {
    log_i("User selected to %s to the last read page", gotoLastReadPage ? "go" : "not go");

    telemetry_.logMetric(TelemetryField::METRIC_DIALOG, kindleBook_->uuid,
                         {{"dialog", std::string("kindle-last-read")},
                          {
                              "action",
                              std::string("last-read-dialog-response"),
                          },
                          {"goto_last_read", gotoLastReadPage},
                          {"kindle", true}});

#ifdef TESTS
    if (onTestLastReadDialogChoice_) {
        log_i("TEST: last-read dialog choice callback invoked (goto=%d)", gotoLastReadPage);
        onTestLastReadDialogChoice_(gotoLastReadPage);
    } else {
        log_w("TEST: last-read dialog choice callback not set");
    }
#endif

    setCurrentScreen(nullptr);

    if (gotoLastReadPage) {
        submitLastReadDialogChoice(true, "Syncing to last read page...");
    } else {
        submitLastReadDialogChoice(false, "Loading content...");
    }
}

void KindleReadableScreen::submitLastReadDialogChoice(bool gotoLastReadPage,
                                                      const std::string &loadingMessage) {
    setState(State::LOADING_INITIAL_CONTENT);
    loadingMessageView_.setText(loadingMessage);
    setNeedsLayout();
    setNeedsRerender();
    render(display_);

    DispatchGetCore0BackgroundQueue()->dispatch([this, gotoLastReadPage]() {
        auto result = SindarinAPI::getInstance()->submitLastReadPageChoice(
            gotoLastReadPage, isUsingBleConnection_, cancelFlag_);

        if (gotoLastReadPage && result != 200) {
            DispatchGetUiQueue()->dispatch([this]() {
                telemetry_.logMetric(
                    TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                    {{"event", std::string("dialog-last-read-error")},
                     {"action", std::string("error")},
                     {"kindle", true},
                     {"error", std::string("Failed to sync to last read page")},
                     {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0}});

                setErrorMessage("Failed to sync to the last read page. Please try again.");
            });
            return;
        }

        // If syncing to latest page, clear the buffer since we're jumping to a new location
        if (gotoLastReadPage) {
            pageFlipper_.clearBuffer();
            DispatchGetUiQueue()->dispatch([this]() {
                currentPlace_ = Place::factory(0);
                newPlace_ = currentPlace_;
                renderer_.setCurrentPlace(currentPlace_);
            });
            setState(State::LOADING_INITIAL_CONTENT);
        }

        pageFlipper_.loadInitialPage();
    });
}

void KindleReadableScreen::scheduleDelayedLoadingMessage() {
    // Cancel any existing delayed message timer
    cancelDelayedLoadingMessage();

    // Create a one-shot timer that fires after 30 seconds
    delayedMessageTimer_ = xTimerCreate("KindleDelayedMsg", pdMS_TO_TICKS(30 * 1000), pdFALSE, this,
                                        delayedMessageTimerCallback);

    xTimerStart(delayedMessageTimer_, 0);
    log_i("Scheduled delayed loading message for 30 seconds");
}

void KindleReadableScreen::cancelDelayedLoadingMessage() {
    if (delayedMessageTimer_ != nullptr) {
        xTimerStop(delayedMessageTimer_, 0);
        xTimerDelete(delayedMessageTimer_, 0);
        delayedMessageTimer_ = nullptr;
        log_d("Cancelled delayed loading message timer");
    }
}

void KindleReadableScreen::delayedMessageTimerCallback(TimerHandle_t timer) {
    auto *instance = static_cast<KindleReadableScreen *>(pvTimerGetTimerID(timer));

    // Execute on UI thread
    DispatchGetUiQueue()->dispatch([instance]() {
        if (instance->state_ != State::LOADING_INITIAL_CONTENT &&
            instance->state_ != State::LOADING_CONTENT) {
            return; // Don't show if we're no longer loading
        }

        // Set and show the delayed message
        instance->delayedMessageView_.setText("This can take up to 60 seconds.");
        instance->delayedMessageView_.setHidden(false);
        instance->setNeedsLayout();
        instance->draw();
        instance->display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true);

        log_i("Showing delayed loading message after 30 seconds");
    });
}

#ifdef DEBUG_KINDLE_RECONNECTION
/*
 * DEBUG: Connection Loss Simulation for Testing Reconnection Behavior
 * ===================================================================
 *
 * To enable simulated connection loss for debugging reconnection issues:
 *
 * 1. At the top of this file, uncomment:
 *    #define DEBUG_KINDLE_RECONNECTION
 *
 * 2. How it works:
 *    - 15 seconds after Kindle content is displayed, it simulates a connection error
 *    - This triggers attemptReconnection() just like a real connection loss would
 *    - Helps debug reconnection loops and retry logic
 *    - Only executes once per session (testDisconnectExecuted_ flag)
 *
 * 3. What to look for in logs:
 *    - "TEST ONLY: Simulating connection loss" messages
 *    - "attemptReconnection called" with attempt counts
 *    - Whether reconnection succeeds or fails
 *    - Any recursive reconnection attempts
 *
 * IMPORTANT: This is DEBUG CODE ONLY - DO NOT SHIP WITH THIS ENABLED!
 */

void KindleReadableScreen::scheduleTestDisconnect() {
    if (testDisconnectExecuted_) {
        log_i("TEST: Forced disconnect already executed once, skipping");
        return;
    }
    cancelTestDisconnect();
    log_e("TEST ONLY: Scheduling simulated connection loss in 15 seconds - REMOVE BEFORE RELEASE!");
    testDisconnectTimer_ = xTimerCreate("TestConnLoss", pdMS_TO_TICKS(15 * 1000), pdFALSE, this,
                                        testDisconnectTimerCallback);
    xTimerStart(testDisconnectTimer_, 0);
    log_i("TEST: Scheduled simulated connection loss for 15 seconds from now");
}

void KindleReadableScreen::cancelTestDisconnect() {
    if (testDisconnectTimer_ != nullptr) {
        xTimerStop(testDisconnectTimer_, 0);
        xTimerDelete(testDisconnectTimer_, 0);
        testDisconnectTimer_ = nullptr;
        log_d("TEST: Cancelled forced disconnect timer");
    }
}

void KindleReadableScreen::testDisconnectTimerCallback(TimerHandle_t timer) {
    auto *instance = static_cast<KindleReadableScreen *>(pvTimerGetTimerID(timer));
    DispatchGetUiQueue()->dispatch([instance]() {
        if (instance->state_ != State::DISPLAY_CONTENT) {
            log_i("TEST: Skipping simulated connection loss - not in DISPLAY_CONTENT state");
            return;
        }
        instance->testDisconnectExecuted_ = true;
        log_e("TEST ONLY: Simulating connection loss to test reconnection behavior!");
        log_e("TEST ONLY: Current state: %s", instance->stateToString(instance->state_));
        log_e("TEST ONLY: Using BLE: %s", instance->isUsingBleConnection_ ? "yes" : "no");
        log_e("TEST ONLY: Reconnection attempts before: %d", instance->reconnectionAttempts_);

        instance->telemetry_.logMetric(
            TelemetryField::METRIC_READABLE_REQUEST, instance->kindleBook_->uuid,
            {{"event", std::string("test-simulated-connection-loss")},
             {"action", std::string("connection-loss-triggered")},
             {"kindle", true},
             {"po",
              instance->currentPlace_ ? static_cast<int>(instance->currentPlace_->getOffset()) : 0},
             {"ble", instance->isUsingBleConnection_}});

        // Trigger reconnection just like the page flipper's error handler does
        instance->attemptReconnection();
        log_e("TEST ONLY: Connection error handler triggered - reconnection should begin");
    });
}
#endif // DEBUG_KINDLE_RECONNECTION

void KindleReadableScreen::attemptReconnection() {
    // Check if we're already in the middle of a reconnection attempt
    if (isReconnecting_) {
        log_i("Already attempting reconnection, ignoring duplicate request");
        return;
    }

    // Set the flag to prevent recursive calls
    isReconnecting_ = true;

    // Increment the reconnection attempts counter
    reconnectionAttempts_++;

    log_i("Reconnection attempt %d of %d", reconnectionAttempts_, MAX_RECONNECTION_ATTEMPTS);

    // If we've exceeded the maximum number of attempts, show an error and stop trying
    if (reconnectionAttempts_ > MAX_RECONNECTION_ATTEMPTS) {
        log_w("Exceeded maximum reconnection attempts (%d). Giving up.", MAX_RECONNECTION_ATTEMPTS);
        isReconnecting_ = false;
        DispatchGetUiQueue()->dispatch([this]() {
            telemetry_.logMetric(
                TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                {{"event", std::string("reconnection")},
                 {"action", std::string("error")},
                 {"kindle", true},
                 {"error", std::string("Exceeded maximum reconnection attempts")},
                 {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0},
                 {"ble", isUsingBleConnection_}});

            if (isUsingBleConnection_) {
                setErrorMessage("Bluetooth connection lost. Please reconnect your phone.");
            } else {
                setErrorMessage("Failed to load content. Please check your network connection.");
            }
        });
        return;
    }

    log_i("Reconnection attempt %d/%d", reconnectionAttempts_, MAX_RECONNECTION_ATTEMPTS);

    // Cancel any existing reconnection attempt
    cancelReconnectionAttempt();

    // Don't show an error message immediately - wait and see if we can reconnect
    // But we should show the loading state to indicate we're trying to reconnect
    DispatchGetUiQueue()->dispatch([this]() {
        if (state_ != State::LOADING_CONTENT) {
            // If we're not already in LOADING_CONTENT state, switch to it
            setState(State::LOADING_CONTENT);
            loadingMessageView_.setText("Reconnecting...");
            setNeedsLayout();
            setNeedsRerender();
            render(display_);
        }
    });

    // Create a new cancel flag for this attempt
    reconnectionAttemptCancelFlag_ = std::make_shared<CancelFlag>();
    // Capture a copy of the flag for the lambda
    auto localFlag = reconnectionAttemptCancelFlag_;

    // Define the callback for reconnection
    auto reconnectionCallback = [this, localFlag](bool connected) {
        // Check if we've been cancelled
        if (CancelFlag::isCancelled(localFlag)) {
            log_i("Reconnection attempt cancelled");
            isReconnecting_ = false;
            return;
        }

        if (connected) {
            log_i("Network reconnected successfully");

            telemetry_.logMetric(
                TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                {{"event", std::string("reconnection")},
                 {"action", std::string("success")},
                 {"kindle", true},
                 {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0},
                 {"ble", isUsingBleConnection_}});

            // Successfully reconnected, retry the operation
            pageFlipper_.retryNetworkOperation();

            // Reset reconnection attempts on successful connection
            reconnectionAttempts_ = 0;
            isReconnecting_ = false;

            // If we have content already (were just reconnecting), restore display state
            if (pageFlipper_.hasContent()) {
                DispatchGetUiQueue()->dispatch([this]() {
                    if (state_ == State::LOADING_CONTENT) {
                        setState(State::DISPLAY_CONTENT);
                        setNeedsLayout();
                        setNeedsRerender();
                        render(display_);
#ifdef TESTS
                        pageFlipper_.notifyTestContentReady();
#endif
                    }
                });
            }
        } else {
            log_w("Failed to reconnect");

            telemetry_.logMetric(
                TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                {{"event", std::string("reconnection")},
                 {"action", std::string("error")},
                 {"kindle", true},
                 {"error", reconnectionAttempts_ >= MAX_RECONNECTION_ATTEMPTS
                               ? std::string("Reconnect failed, out of attempts")
                               : std::string("Reconnect failed")},
                 {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0},
                 {"ble", isUsingBleConnection_},
                 {"attempt", reconnectionAttempts_}});

            // Reset the flag to allow future reconnection attempts
            isReconnecting_ = false;

            // If we've reached max attempts, show appropriate error
            if (reconnectionAttempts_ >= MAX_RECONNECTION_ATTEMPTS) {
                DispatchGetUiQueue()->dispatch([this]() {
                    setErrorMessage(
                        "Failed to load content. Please check your network connection.");
                });
            }
        }
    };

    // Use a short delay to allow the connection to potentially recover
    DispatchGetCore0BackgroundQueue()->dispatch([this, localFlag, reconnectionCallback]() {
        // Wait a moment before attempting reconnection
        vTaskDelay(
            pdMS_TO_TICKS(3000)); // 3 second delay - give more time for the network stack to reset

        // Check if we've been cancelled
        if (CancelFlag::isCancelled(localFlag)) {
            log_i("Reconnection attempt cancelled");
            return;
        }

        telemetry_.logMetric(
            TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
            {{"event", std::string("reconnection")},
             {"action", std::string("begin")},
             {"kindle", true},
             {"attempts", reconnectionAttempts_},
             {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0},
             {"ble", isUsingBleConnection_}});

        std::string errorMessage;
        int bleReconnectTimeoutMs = 10000; // Shorter timeout for reconnection
        connectToNetwork(errorMessage, reconnectionCallback, bleReconnectTimeoutMs);
    });
}

void KindleReadableScreen::cancelReconnectionAttempt() {
    if (reconnectionAttemptCancelFlag_ == nullptr ||
        CancelFlag::isCancelled(reconnectionAttemptCancelFlag_)) {
        return;
    }

    CancelFlag::cancel(reconnectionAttemptCancelFlag_);
    reconnectionAttemptCancelFlag_ = nullptr;
}

void KindleReadableScreen::setCurrentScreen(const std::shared_ptr<Screen> &screen) {
    if (currentScreen_) {
        currentScreen_->setFirstLoad();
    }
    if (screen) {
        screen->setNeedsRerender();
    } else {
        updateType_ = DisplaySystem::UpdateType::PARTIAL;
        setNeedsRerender();
    }
    currentScreen_ = screen;
}

void KindleReadableScreen::updatePageFlipperCallbacks() {
    auto cancelFlag = cancelFlag_;

    pageFlipper_.setOnContentReady([this, cancelFlag](size_t ocrTextLength) {
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }
        DispatchGetUiQueue()->dispatch([this, cancelFlag, ocrTextLength]() {
            if (CancelFlag::isCancelled(cancelFlag)) {
                return;
            }
            if (state_ == State::LOADING_INITIAL_CONTENT || state_ == State::LOADING_CONTENT ||
                state_ == State::DISPLAY_PARTIAL_CONTENT) {
                // Only log open-success telemetry for the initial content load
                if (state_ == State::LOADING_INITIAL_CONTENT) {
                    int64_t elapsedMs = (esp_timer_get_time() - bookOpenStartTime_) / 1000;
                    telemetry_.logMetric(
                        TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                        {{"event", std::string("book-open")},
                         {"action", std::string("open-success")},
                         {"kindle", true},
                         {"po", currentPlace_ ? std::to_string(currentPlace_->getOffset()) : "0"},
                         {"elapsed_ms", static_cast<int>(elapsedMs)},
                         {"ocr_text_length", static_cast<int>(ocrTextLength)},
                         {"state", std::string(stateToString(state_))}});
                }

                setState(State::DISPLAY_CONTENT);
                setNeedsLayout();
                setNeedsRerender();
                render(display_);
#ifdef TESTS
                pageFlipper_.notifyTestContentReady();
#endif

#ifdef DEBUG_KINDLE_RECONNECTION
                scheduleTestDisconnect();
#endif
            }
        });
    });

    pageFlipper_.setOnErrorFunction([this, cancelFlag](const std::string &error) {
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }
        DispatchGetUiQueue()->dispatch([this, error, cancelFlag]() {
            if (CancelFlag::isCancelled(cancelFlag)) {
                return;
            }

            // Only show error message for non-connection errors
            // CONNECTION_ERROR is handled by the onConnectionErrorFunction below
            if (error != "CONNECTION_ERROR") {
                // Determine if this is an open or navigate error based on current state
                bool isInitialLoad = (state_ == State::LOADING_INITIAL_CONTENT);

                // Use appropriate timing based on whether we're opening or navigating
                int64_t startTime = isInitialLoad ? bookOpenStartTime_ : navigationStartTime_;
                int64_t elapsedMs = (esp_timer_get_time() - startTime) / 1000;
                telemetry_.logMetric(
                    TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                    {{"event",
                      isInitialLoad ? std::string("book-open") : std::string("book-navigate")},
                     {"action", isInitialLoad ? std::string("open-failed-server")
                                              : std::string("navigate-failed-server")},
                     {"kindle", true},
                     {"error", error},
                     {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0},
                     {"ble", isUsingBleConnection_},
                     {"elapsed_ms", static_cast<int>(elapsedMs)},
                     {"state", std::string(stateToString(state_))}});

                setErrorMessage(error);
            }
        });
    });

    // Set up the callback for last read dialog
    pageFlipper_.setOnLastReadDialogFunction([this, cancelFlag](const SpiramString &dialogText) {
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }
        DispatchGetUiQueue()->dispatch([this, dialogText, cancelFlag]() {
            if (CancelFlag::isCancelled(cancelFlag)) {
                return;
            }

            // Cancel loading message and clear loading state before showing dialog
            cancelDelayedLoadingMessage();

            // Show the last read dialog
            showLastReadDialog(dialogText);
        });
    });

    // Set a separate callback specifically for network connectivity issues
    pageFlipper_.setOnConnectionErrorFunction([this, cancelFlag]() {
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }
        DispatchGetUiQueue()->dispatch([this, cancelFlag]() {
            if (CancelFlag::isCancelled(cancelFlag)) {
                return;
            }

            // Log telemetry for connection errors
            // Determine if this is an open or navigate error based on current state
            bool isInitialLoad = (state_ == State::LOADING_INITIAL_CONTENT);

            // Use appropriate timing based on whether we're opening or navigating
            int64_t startTime = isInitialLoad ? bookOpenStartTime_ : navigationStartTime_;
            int64_t elapsedMs = (esp_timer_get_time() - startTime) / 1000;
            telemetry_.logMetric(
                TelemetryField::METRIC_READABLE_REQUEST, kindleBook_->uuid,
                {{"event", isInitialLoad ? std::string("book-open") : std::string("book-navigate")},
                 {"action", isInitialLoad ? std::string("open-failed-connection")
                                          : std::string("navigate-failed-connection")},
                 {"kindle", true},
                 {"error", std::string("connection-error")},
                 {"po", currentPlace_ ? static_cast<int>(currentPlace_->getOffset()) : 0},
                 {"ble", isUsingBleConnection_},
                 {"elapsed_ms", static_cast<int>(elapsedMs)},
                 {"state", std::string(stateToString(state_))}});

            // If we have a connection error, silently try to reconnect
            log_i("Network connection issue detected, attempting reconnection");

            attemptReconnection();
        });
    });
}
