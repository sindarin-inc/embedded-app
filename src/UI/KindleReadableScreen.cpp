#include "UI/KindleReadableScreen.hpp"

#include <cinttypes>

#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEManager.hpp"
#endif
#include "Misc/StringUtil.hpp"
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
}

void KindleReadableScreen::render(DisplaySystem &display) {
    if (state_ == State::DISPLAY_CONTENT) {
        // Set up the frame and layout
        setFrame(defaultFrameRect());
        setNeedsLayout();

        // Draw the basic screen (background, etc.)
        draw();

        // Render the current page content
        uint8_t darkPicturePercent = 0;
        std::shared_ptr<Place> newPlace;
        std::tie(newPlace, darkPicturePercent) = renderer_.renderPage(renderer_.getCurrentPlace());

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
    // If in an error state, any button press dismisses
    if (state_ == State::ERROR) {
        if (command == RemoteCommand::ENTER || command == RemoteCommand::BACK) {
            dismissScreen();
            return;
        }
    }

    // If connecting or loading, allow cancellation
    if (state_ == State::CONNECTING || state_ == State::LOADING_CONTENT) {
        if (command == RemoteCommand::BACK) {
            dismissScreen();
            return;
        }
        // Ignore other commands while loading
        return;
    }

    if (state_ == State::DISPLAY_CONTENT) {
        // Handle navigation commands specially for Kindle content
        switch (command) {
        case RemoteCommand::RIGHT:
            navigateNext();
            break;
        case RemoteCommand::LEFT:
            navigatePrevious();
            break;
        case RemoteCommand::BACK:
        case RemoteCommand::UP:
            // TODO: Implement popup menu for Kindle books
            dismissScreen();
            break;
        case RemoteCommand::ENTER:
            // No-op for now, could be used for future features
            break;
        case RemoteCommand::DOWN:
            // No-op for now, could be used for future features
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
                    if (dismissed_) return;

                    errorMessageView_.setText(errorMessage);
                    setState(State::ERROR);
                    setNeedsRerender();
                });
                return;
            }

            // Successfully connected, now load the book content
            DispatchGetUiQueue()->dispatch([this]() {
                if (dismissed_) return;

                setState(State::LOADING_CONTENT);
                setLoadingMessage("Loading Kindle book...");
                setNeedsRerender();

                // Now load the content
                loadKindleBookPage();
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

    // Construct the URL based on whether this is initial load or navigation
    std::string url;
    if (firstKindleLoad_) {
        url = "/kindle/open_book?title=" + kindleBook_->title;
    } else {
        url = "/kindle/navigate-next";
    }

    log_i("Making request to URL: %s", url.c_str());

    // Create HTTP client
    auto client =
        SindarinAPI::createClient(url, "application/json", HTTP_METHOD_GET, false, cancelFlag_);
    if (!client) {
        log_e("Failed to create HTTP client");
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Failed to create network client");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Set authentication
    auto err = SindarinAPI::getInstance()->setAuth(client);
    if (err != ESP_OK) {
        log_e("Failed to set authentication: %d", err);
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Authentication error");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Open the client
    err = client->open();
    if (err != ESP_OK) {
        log_e("Failed to open HTTP client: %d", err);
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Connection error");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Fetch headers
    err = client->fetchHeaders();
    if (err != ESP_OK) {
        log_e("Failed to fetch HTTP headers: %d", err);
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Network error");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    int status = client->getStatusCode();
    if (status != 200) {
        log_e("HTTP request failed with status code: %d", status);
        DispatchGetUiQueue()->dispatch([this, status]() {
            if (dismissed_) return;

            errorMessageView_.setText(StringFormat("Server error (%d)", status));
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Read the response
    auto stream = client->makeReadStream();
    if (!stream) {
        log_e("Failed to create read stream");
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Failed to read server response");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Buffer to read the JSON response
    std::string responseJson;
    char buf[1024];
    while (stream->read(buf, sizeof(buf) - 1)) {
        int bytesRead = stream->gcount();
        if (bytesRead <= 0) {
            break;
        }
        buf[bytesRead] = 0;
        responseJson += buf;
    }

    if (responseJson.empty()) {
        log_e("No response data received from server");
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("No response from server");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    log_i("Received response: %s", responseJson.c_str());

    // Process the response on the UI thread
    DispatchGetUiQueue()->dispatch([this, responseJson]() {
        if (dismissed_) return;

        processPageResponse(responseJson);
    });
}

void KindleReadableScreen::navigateNext() {
    if (state_ != State::DISPLAY_CONTENT || dismissed_) {
        return;
    }

    setState(State::LOADING_CONTENT);
    setLoadingMessage("Loading next page...");
    setNeedsRerender();

    // Create HTTP client
    auto client = SindarinAPI::createClient("/kindle/navigate-next", "application/json",
                                            HTTP_METHOD_GET, false, cancelFlag_);
    if (!client) {
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Failed to create network client");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Set authentication
    auto err = SindarinAPI::getInstance()->setAuth(client);
    if (err != ESP_OK) {
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Authentication error");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Open the client
    err = client->open();
    if (err != ESP_OK) {
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Connection error");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Fetch headers
    err = client->fetchHeaders();
    if (err != ESP_OK) {
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Network error");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    int status = client->getStatusCode();
    if (status != 200) {
        DispatchGetUiQueue()->dispatch([this, status]() {
            if (dismissed_) return;

            errorMessageView_.setText(StringFormat("Server error (%d)", status));
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Read the response
    auto stream = client->makeReadStream();
    if (!stream) {
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Failed to read server response");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Buffer to read the JSON response
    std::string responseJson;
    char buf[1024];
    while (stream->read(buf, sizeof(buf) - 1)) {
        int bytesRead = stream->gcount();
        if (bytesRead <= 0) {
            break;
        }
        buf[bytesRead] = 0;
        responseJson += buf;
    }

    // Process the response on the UI thread
    DispatchGetUiQueue()->dispatch([this, responseJson]() {
        if (dismissed_) return;

        processPageResponse(responseJson);
    });
}

void KindleReadableScreen::navigatePrevious() {
    if (state_ != State::DISPLAY_CONTENT || dismissed_) {
        return;
    }

    setState(State::LOADING_CONTENT);
    setLoadingMessage("Loading previous page...");
    setNeedsRerender();

    // Create HTTP client
    auto client = SindarinAPI::createClient("/kindle/navigate-previous", "application/json",
                                            HTTP_METHOD_GET, false, cancelFlag_);
    if (!client) {
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Failed to create network client");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Set authentication
    auto err = SindarinAPI::getInstance()->setAuth(client);
    if (err != ESP_OK) {
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Authentication error");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Open the client
    err = client->open();
    if (err != ESP_OK) {
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Connection error");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Fetch headers
    err = client->fetchHeaders();
    if (err != ESP_OK) {
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Network error");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    int status = client->getStatusCode();
    if (status != 200) {
        DispatchGetUiQueue()->dispatch([this, status]() {
            if (dismissed_) return;

            errorMessageView_.setText(StringFormat("Server error (%d)", status));
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Read the response
    auto stream = client->makeReadStream();
    if (!stream) {
        DispatchGetUiQueue()->dispatch([this]() {
            if (dismissed_) return;

            errorMessageView_.setText("Failed to read server response");
            setState(State::ERROR);
            setNeedsRerender();
        });
        return;
    }

    // Buffer to read the JSON response
    std::string responseJson;
    char buf[1024];
    while (stream->read(buf, sizeof(buf) - 1)) {
        int bytesRead = stream->gcount();
        if (bytesRead <= 0) {
            break;
        }
        buf[bytesRead] = 0;
        responseJson += buf;
    }

    // Process the response on the UI thread
    DispatchGetUiQueue()->dispatch([this, responseJson]() {
        if (dismissed_) return;

        processPageResponse(responseJson);
    });
}

void KindleReadableScreen::processPageResponse(const std::string &response) {
    // Parse the JSON response
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        log_e("Failed to parse Kindle page response: %s", error.c_str());
        errorMessageView_.setText("Failed to parse server response");
        setState(State::ERROR);
        setNeedsRerender();
        return;
    }

    // Check for success
    bool success = doc["success"];
    if (!success) {
        log_e("Kindle book request failed: %s", response.c_str());
        errorMessageView_.setText("Failed to load Kindle book content");
        setState(State::ERROR);
        setNeedsRerender();
        return;
    }

    // Get progress information
    JsonObject progress = doc["progress"];
    currentPage_ = progress["current_page"];
    totalPages_ = progress["total_pages"];
    int progressPercentage = progress["percentage"];

    // Get page content
    currentPageText_ = doc["ocr_text"].as<std::string>();
    currentPageImageUrl_ = doc["screenshot_url"].as<std::string>();

    log_i("Kindle page loaded: %d/%d (%d%%)", currentPage_, totalPages_, progressPercentage);

    // Update the book progress in the KindleBook object
    kindleBook_->progressPercent = progressPercentage;
    kindleBook_->progress = StringFormat("%d%%", progressPercentage);

    // Create a stream with the text content
    auto stream = std::make_shared<StdStringStream>(currentPageText_, DocType::TXT);
    renderer_.setStream(stream);

    // Create a place for the content
    auto place = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));

    // Set up display content state
    setState(State::DISPLAY_CONTENT);

    // Render the page
    uint8_t darkPicturePercent = 0;
    std::shared_ptr<Place> newPlace;
    std::tie(newPlace, darkPicturePercent) = renderer_.renderPage(place);

    // Update state and trigger render
    firstKindleLoad_ = false;
    setNeedsRerender();
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
