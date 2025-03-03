#include "ContentSyncScreen.hpp"

#include <freertos/FreeRTOS.h>
#if !SIMULATOR
#include "Communications/BLEManager.hpp"
#endif
#include "Communications/WiFi.hpp"
#include "Misc/Platform.hpp"
#include "Misc/RateLimiter.hpp"
#include "Renderers/RendererStream.hpp"

ContentSyncScreen::ContentSyncScreen(DisplaySystem &display, LocalStore &localStore,
                                     PreferencesStore &prefs, Telemetry &telemetry,
                                     PowerManager &power)
    : EinkView(display), localStore_(localStore), prefs_(prefs), telemetry_(telemetry),
      power_(power), cancelView_(display, "Press \ue000 to cancel", &fontFace0) {
    for (auto &textView : textViews_) {
        textView = std::make_shared<SingleLineTextView>(display, "", &fontFace0);
    }
    for (auto &progressBarView : progressBarsViews_) {
        progressBarView = std::make_shared<ProgressBarView>(display);
    }
    for (auto &checkmarkView : checkmarkViews_) {
        checkmarkView =
            std::make_shared<IconView>(display, CHECK_MARK_BITS, CheckMark_width, CheckMark_height);
        checkmarkView->setHidden(true);
    }
    (*textViews_[0]).setText("Connect to internet");
    (*textViews_[1]).setText("Prepare downloads");
    (*textViews_[2]).setText("Download books");
    (*textViews_[3]).setText("Download articles");

    popupView_ = std::make_shared<BoxView>(display, 5);
    popupView_->fillColor = display.bg;
    popupView_->setHidden(true);
    popupTextView_ = std::make_shared<MultiLineTextView>(display, 10);
    popupTextView_->alignCenter = true;
    popupTextView_->dynamicWidth = true;
    popupTextView_->setHidden(true);

    power_.addStateChangeCallback(this, [this](PowerState state) {
        if (state == PowerState::ShuttingDown) {
            log_d("Signaling content sync to cancel");
            CancelFlag::cancel(cancel_);
        }
    });
}

ContentSyncScreen::~ContentSyncScreen() { power_.removeStateChangeCallback(this); }

auto ContentSyncScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    context.setSublayoutFrame(
        SolRect::fromSize(size), LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, SolSize size) -> SolSize {
            int x = 0;
            int y = 0;

            for (int i = 0; i < 4; i++) {
                auto textView = textViews_[i];
                auto checkmarkView = checkmarkViews_[i];
                auto progressBarView = progressBarsViews_[i];
                auto rect = context.setViewFrame(*textView, {{x, y}, {size.width, size.height}},
                                                 LayoutOptionsSizeToFitVertical);
                context.setViewFrame(*checkmarkView, rect,
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignRight |
                                         LayoutOptionsAlignCenterVertical);
                y += rect.size.height;
                y += 5;

                context.setViewFrame(*progressBarView, {{x, y}, {size.width, size.height}},
                                     LayoutOptionsSizeToFitVertical);
                y += rect.size.height;
                y += 10;
            }
            y -= 10;

            return {size.width, y};
        });

    if (!popupView_->isHidden()) {
        auto popupTextRect =
            context.setViewSizeInRect(*popupTextView_, {150, 150}, SolRect::fromSize(size),
                                      LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
        context.setViewFrame(*popupView_, popupTextRect.inset(-10));
    }

    context.setViewFrame(cancelView_, SolRect::fromSize(size).inset(10),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);

    return {size.width, size.height};
}

void ContentSyncScreen::drawInRect(SolRect rect) {
    for (const auto &textView : textViews_) {
        (*textView).draw();
    }
    for (const auto &progressBarView : progressBarsViews_) {
        (*progressBarView).draw();
    }
    for (const auto &checkmarkView : checkmarkViews_) {
        (*checkmarkView).draw();
    }
    if (!popupView_->isHidden()) {
        (*popupView_).draw();
    }
    if (!popupTextView_->isHidden()) {
        (*popupTextView_).draw();
    }
    cancelView_.draw();
}

void ContentSyncScreen::showPopup(const std::string &message) {
    popupTextView_->setText(message);
    popupView_->setHidden(false);
    popupTextView_->setHidden(false);
    setNeedsLayout();
    nextUpdate_ = DisplaySystem::UpdateType::PARTIAL;
}

void ContentSyncScreen::cancel() {
    CancelFlag::cancel(cancel_);
    telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-cancelled");
    WiFi::getInstance().disconnectWiFi();
#if CONFIG_USE_BLE_SERVER_PROXY
    auto bleManager = BLEManager::getInstance(false);
    if (bleManager != nullptr) {
        bleManager->disconnectProxy();
    }
#endif
    showPopup("Sync cancelled!\n\nRestarting...");
    render(display_);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    power_.reboot();
}

void ContentSyncScreen::render(DisplaySystem &display) {
    int xPadding = 42;
    int yPadding = 10;
    setFrame(SolRect::fromSize({display.width(), display.height()}).inset(xPadding, yPadding));

    // Re-render based on state
    switch (state_.state) {
    case ContentSyncState::Idle: {
        break;
    }
    case ContentSyncState::ConnectingWifi:
        progressBarsViews_[0]->setProgress(0);
        break;
    case ContentSyncState::UploadTelemetry:
        progressBarsViews_[0]->setProgress(100);
        checkmarkViews_[0]->setHidden(false);
        break;
    case ContentSyncState::CheckingSync:
        progressBarsViews_[0]->setProgress(100);
        progressBarsViews_[1]->setProgress(50);
        checkmarkViews_[0]->setHidden(false);
        break;
    case ContentSyncState::SyncingBooks:
        progressBarsViews_[0]->setProgress(100);
        progressBarsViews_[1]->setProgress(100);
        checkmarkViews_[0]->setHidden(false);
        checkmarkViews_[1]->setHidden(false);
        progressBarsViews_[2]->setProgress(state_.percentage);
        break;
    case ContentSyncState::SyncingArticles:
        progressBarsViews_[0]->setProgress(100);
        progressBarsViews_[1]->setProgress(100);
        progressBarsViews_[2]->setProgress(100);
        checkmarkViews_[0]->setHidden(false);
        checkmarkViews_[1]->setHidden(false);
        checkmarkViews_[2]->setHidden(false);
        progressBarsViews_[3]->setProgress(state_.percentage);
        break;
    case ContentSyncState::AlreadySynced: {
        progressBarsViews_[0]->setProgress(100);
        progressBarsViews_[1]->setProgress(100);
        progressBarsViews_[2]->setProgress(100);
        progressBarsViews_[3]->setProgress(100);
        checkmarkViews_[0]->setHidden(false);
        checkmarkViews_[1]->setHidden(false);
        checkmarkViews_[2]->setHidden(false);
        checkmarkViews_[3]->setHidden(false);

        std::string message = "Already in sync!";
        showPopup(message);
        break;
    }
    case ContentSyncState::Synced: {
        progressBarsViews_[0]->setProgress(100);
        progressBarsViews_[1]->setProgress(100);
        progressBarsViews_[2]->setProgress(100);
        progressBarsViews_[3]->setProgress(100);
        checkmarkViews_[0]->setHidden(false);
        checkmarkViews_[1]->setHidden(false);
        checkmarkViews_[2]->setHidden(false);
        checkmarkViews_[3]->setHidden(false);

        std::string message = "Sync completed!\n\n";
        if (state_.booksDownloaded > 0) {
            message += StringFormat("%d books added\n", state_.booksDownloaded);
        }
        if (state_.articlesDownloaded > 0) {
            message += StringFormat("%d articles added\n", state_.articlesDownloaded);
        }
        if (state_.booksDownloaded > 0 || state_.articlesDownloaded > 0) {
            message += "\n";
        }
        message += "Restarting...";
        showPopup(message);

        break;
    }
    case ContentSyncState::Failure:
        showPopup(StringFormat("Sync unsuccessful\n%s\n\nRestarting...", state_.message.c_str()));
        break;
    }

    draw();

    display.updateDisplay(nextUpdate_);

    nextUpdate_ = DisplaySystem::UpdateType::FAST_INDICATOR;
}

void ContentSyncScreen::processCommand(RemoteCommand command) {
    if (command == RemoteCommand::ENTER) {
        cancel();
    }
}

void ContentSyncScreen::fail(const std::string &message) {
    log_w("Content sync failed: %s", message.c_str());

    this->telemetry_.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                               {{"message", message}});
    state_.state = ContentSyncState::Failure;
    state_.message = message;
    render(display_);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    WiFi::getInstance().disconnectWiFi();
#if CONFIG_USE_BLE_SERVER_PROXY
    auto bleManager = BLEManager::getInstance(false);
    if (bleManager != nullptr) {
        bleManager->disconnectProxy();
    }
#endif
    power_.reboot();
}

void ContentSyncScreen::begin() {
    // If we're in first time mode, we need to make sure we have a cloud token and WiFi
    // credentials before we can do anything.
    if (!prefs_.cloudToken.get().has_value()) {
        fail("Go to Settings > Pair Glasses in the Sol app.");
        return;
    }

    if (!WiFi::getInstance().hasWiFiCredentials()) {
        fail("Go to Settings > WiFi Setup in the Sol app.");
        return;
    }

#if USE_BLE_SERVER
    // Disable the BLE Peripheral. This seems to almost always crash the device with a
    // btdm_controller_task crash
    BLEManager::getInstance()->stopAdvertising();
    auto bleManager = BLEManager::getInstance();
    // Check if already connected to sync service, if not, disconnect all clients
    if (!bleManager->connectedProxy) {
        bleManager->disconnectAllClients();
    } else {
        log_d("Already connected to sync service, not disconnecting clients");
    }
    auto resumeBLE = [bleManager] { bleManager->startAdvertisingIfNecessary(); };
#else
    auto resumeBLE = [] {};
#endif

    // Rate limit to not update more than every 1s, and only if the percentage has changed
    auto rl = std::make_shared<RateLimiter>(1, 20, 1);
    cancel_ = std::make_shared<CancelFlag>();
    DispatchGetCore0BackgroundQueue()->dispatch([this, rl, resumeBLE]() {
        auto progressCallback = [this, rl, resumeBLE](const ContentSyncState &state) {
            log_d("Progress callback: %d, %d%%", state.state, state.percentage);

            if (CancelFlag::isCancelled(cancel_)) {
                return;
            }

            power_.feedInactivityTimer();
            if (state.state == ContentSyncState::Failure ||
                state.state == ContentSyncState::Synced ||
                state.state == ContentSyncState::AlreadySynced) {
                log_d("Content sync failed, disconnecting WiFi and BLE");
                WiFi::getInstance().disconnectWiFi();
#if CONFIG_USE_BLE_SERVER_PROXY
                auto bleManager = BLEManager::getInstance(false);
                if (bleManager != nullptr) {
                    bleManager->disconnectProxy();
                }
#endif
            }
            // Skip updates if the state is the same and the percentage isn't different (or if
            // not enough time has elapsed)
            if (state.state == state_.state) {
                if (!rl->shouldExecute(state.percentage)) {
                    return;
                }
            } else {
                rl->resetPercentage();
            }

            // Do UI stuff back on core 1
            log_d("Dispatching Content Sync UI update for state %d", state.state);
#if CONFIG_USE_BLE_SERVER_PROXY
            auto tryWithBle = !state.usedBle;
#else
            auto tryWithBle = false;
#endif
            if (state.state == ContentSyncState::Failure && tryWithBle) {
                log_d("Content sync with wifi failed, trying again with BLE");
            } else {
                DispatchGetUiQueue()->dispatch([this, state, resumeBLE]() {
                    this->updateState(state);
                    this->render(this->display_);

                    if (state.state == ContentSyncState::Synced) {
                        log_d("Content sync done, rebooting");
                        Delay(2000);
                        power_.reboot();
                    } else if (state.state == ContentSyncState::AlreadySynced) {
                        log_d("Content sync already synced");
                        Delay(2000);
                        resumeBLE();
                        back();
                    } else if (state.state == ContentSyncState::Failure) {
                        // If we got any new content force a restart, otherwise just go back
                        if (state.booksDownloaded > 0 || state.articlesDownloaded > 0) {
                            log_d("Content sync failed, but we got new content, rebooting");
                            Delay(2000);
                            power_.reboot();
                        } else {
                            log_d("Content sync failed, going back");
                            Delay(2000);
                            resumeBLE();
                            back();
                        }
                    }
                });
            }
        };

        // Connect to WiFi
        progressCallback(ContentSyncState{.state = ContentSyncState::State::ConnectingWifi});

        bool wifiConnected = false;
        bool bleConnected = false;
        if (!WiFi::getInstance().connectWiFiFromSettings()) {
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "sync-wifi-failed");
#if !CONFIG_USE_BLE_SERVER_PROXY
            progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                              .message = "Failed to connect to WiFi."});
            return;
#endif
        } else {
            wifiConnected = true;
        }

        auto syncSuccessful = false;
        if (wifiConnected) {
            syncSuccessful = localStore_.syncFromRemote(
                wifiConnected, bleConnected, SyncReason::Manual, cancel_, progressCallback);
        }

        log_d("Sync successful: %d, wifi connected: %d, attempting BLE sync", syncSuccessful,
              wifiConnected);
#if CONFIG_USE_BLE_SERVER_PROXY
        if (!syncSuccessful || !wifiConnected) {
            if (!BLEManager::getInstance()->connectProxy(cancel_)) {
                telemetry_.logMetric(TelemetryField::METRIC_WIFI, "sync-ble-failed");
                progressCallback(ContentSyncState{.state = ContentSyncState::State::Failure,
                                                  .usedBle = true,
                                                  .message = "Failed to connect to WiFi or BLE."});
                return;
            } else {
                bleConnected = true;
                wifiConnected = false;
            }
            WiFi::getInstance().disconnectWiFi(3000);
            localStore_.syncFromRemote(wifiConnected, bleConnected, SyncReason::Manual, cancel_,
                                       progressCallback);
        }
#endif
    });
}
