#include "UI/KindleReadableScreen.hpp"

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEManager.hpp"
#endif
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
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
      cancelFlag_(std::make_shared<CancelFlag>()), pageFlipper_(kindleBook, cancelFlag_),
      loadingMessageView_(display, 5), delayedMessageView_(display, 5),
      errorMessageView_(display, 5) {

    log_i("Created KindleReadableScreen for book: %s by %s", kindleBook->title.c_str(),
          kindleBook->author.c_str());

    currentPlace_ = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));

    auto cancelFlag = cancelFlag_;
    pageFlipper_.setGotContentFunction([this, cancelFlag]() {
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }
        if (state_ == State::LOADING_CONTENT || state_ == State::DISPLAY_PARTIAL_CONTENT) {
            DispatchGetUiQueue()->dispatch([this, cancelFlag]() {
                if (CancelFlag::isCancelled(cancelFlag)) {
                    return;
                }
                setState(State::DISPLAY_CONTENT);
                setNeedsLayout();
                setNeedsRerender();
                render(display_);
            });
        }
    });

    pageFlipper_.setOnErrorFunction([this, cancelFlag](const std::string &error) {
        if (CancelFlag::isCancelled(cancelFlag)) {
            return;
        }
        DispatchGetUiQueue()->dispatch([this, error, cancelFlag]() {
            if (CancelFlag::isCancelled(cancelFlag)) {
                return;
            }
            setErrorMessage(error);
        });
    });

    renderer_.setStream(pageFlipper_.getStream());

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
    popupMenuScreen_->populateMenuView();
}

void KindleReadableScreen::begin() {
    // Start connection process
    DispatchGetCore0BackgroundQueue()->dispatch([this]() {
        std::string errorMessage;
        bool connected = connectToNetwork(false, errorMessage);

        if (!connected) {
            DispatchGetUiQueue()->dispatch(
                [this, errorMessage]() { setErrorMessage(errorMessage); });
            return;
        }

        // Successfully connected, now load the book content
        auto cancelFlag = cancelFlag_;
        DispatchGetUiQueue()->dispatch([this, cancelFlag]() {
            if (CancelFlag::isCancelled(cancelFlag)) {
                return;
            }

            log_d("Setting loading message to \"Opening your Kindle book...\"");

            loadingMessageView_.setText("Opening your Kindle book...");
            setNeedsLayout();
            setState(State::LOADING_CONTENT);
            render(display_);

            // Schedule a delayed message after 10 seconds if loading takes too long
            scheduleDelayedLoadingMessage();
        });

        // Now load the content in the background queue
        pageFlipper_.loadInitialPage();
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
        if (renderer_.atEndOfFile()) {
            setState(State::DISPLAY_PARTIAL_CONTENT);
        }
        pageFlipper_.updatePlace(currentPlace_);

        // Log the current and next place offsets for debugging
        uint32_t currentOffset = currentPlace_->getOffset();
        uint32_t nextOffset = newPlace_->getOffset();
        uint32_t totalSize = renderer_.size();
        log_i("Reading at offset: %" PRIu32 "-%" PRIu32 ", total size: %" PRIu32 " bytes",
              currentOffset, nextOffset, totalSize);
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
    case State::LOADING_CONTENT:
        return "LOADING_CONTENT";
    case State::ERROR:
        return "ERROR";
    case State::DISPLAY_CONTENT:
        return "DISPLAY_CONTENT";
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
    errorMessageView_.setText(message);
    setState(State::ERROR);
    setNeedsLayout();
    setNeedsRerender();
    render(display_);

    // TODO: Re-add auto-dismiss?
}

auto KindleReadableScreen::connectToNetwork(bool useBleOnly, std::string &errorMessage) -> bool {
    log_d("Connecting to network for Kindle book");
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

void KindleReadableScreen::navigateNext() {
    // Transition to loading state if we're showing partial content
    if (state_ == State::DISPLAY_PARTIAL_CONTENT) {
        setState(State::LOADING_CONTENT);
        loadingMessageView_.setText("Loading next page...");
        setNeedsLayout();
        setNeedsRerender();
        return;
    }

    currentPlace_ = newPlace_;
    updateType_ = prefs_.fastTransitions.get().value_or(false) ? DisplaySystem::UpdateType::PARTIAL
                                                               : DisplaySystem::UpdateType::FULL;
    setNeedsRerender();
}

void KindleReadableScreen::navigatePrevious() {
    currentPlace_ = renderer_.getPreviousPlace();
    updateType_ = prefs_.fastTransitions.get().value_or(false) ? DisplaySystem::UpdateType::PARTIAL
                                                               : DisplaySystem::UpdateType::FULL;
    setNeedsRerender();
}

void KindleReadableScreen::dismissScreen() {
    log_d("Dismissing KindleReadableScreen");
    CancelFlag::cancel(cancelFlag_);
    cancelDelayedLoadingMessage();
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
    setNeedsRerender();
}

void KindleReadableScreen::scheduleDelayedLoadingMessage() {
    // Cancel any existing delayed message
    cancelDelayedLoadingMessage();

    // Create a new cancel flag for this delayed message
    delayedMessageCancelFlag_ = std::make_shared<CancelFlag>();
    // Capture a copy of the flag for the lambda
    auto localFlag = delayedMessageCancelFlag_;

    // TODO(john): Timers would be better here. Maybe we could make a DispatchAfter method
    DispatchGetCore0BackgroundQueue()->dispatch([this, localFlag]() {
        // Wait for 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));

        // Check if the task was cancelled
        if (CancelFlag::isCancelled(localFlag)) {
            return;
        }

        // Display the message on the UI thread
        DispatchGetUiQueue()->dispatch([this]() {
            if (state_ != State::LOADING_CONTENT) {
                return; // Don't show if we're no longer loading or screen was dismissed
            }

            // Set and show the delayed message
            delayedMessageView_.setText("This can take up to 30 seconds.");
            delayedMessageView_.setHidden(false);
            setNeedsLayout();

            // Call setNeedsLayout to recalculate positions of all views
            draw();
            display_.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true);

            log_i("Showing delayed loading message after 10 seconds");
        });
    });
}

void KindleReadableScreen::cancelDelayedLoadingMessage() {
    if (delayedMessageCancelFlag_ == nullptr ||
        CancelFlag::isCancelled(delayedMessageCancelFlag_)) {
        return;
    }

    CancelFlag::cancel(delayedMessageCancelFlag_);
    delayedMessageCancelFlag_ = nullptr;
}
