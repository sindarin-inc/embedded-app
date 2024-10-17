#include "FirmwareUpdateScreen.hpp"

#if !SIMULATOR
#include "Communications/BLEManager.hpp"
#endif
#include "Communications/WiFi.hpp"
#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/RateLimiter.hpp"
#include "UI/Bitmaps/CheckMark.xbm"
#include "Version.hpp"

FirmwareUpdateScreen::FirmwareUpdateScreen(DisplaySystem &display, Telemetry &telemetry,
                                           PreferencesStore &prefs)
    : EinkView(display), telemetry_(telemetry), firmwareDownloader_(prefs),
      cancelView_(display, "Press \ue000 to cancel", &fontFace0) {
    for (auto &textView : textViews_) {
        textView = std::make_shared<SingleLineTextView>(display, "", &fontFace0);
    }
    for (auto &progressBarView : progressBarViews_) {
        progressBarView = std::make_shared<ProgressBarView>(display);
    }
    for (auto &checkmarkView : checkmarkViews_) {
        checkmarkView =
            std::make_shared<IconView>(display, CHECK_MARK_BITS, CheckMark_width, CheckMark_height);
        checkmarkView->setHidden(true);
    }
    textViews_[0]->setText("Connecting WiFi");
    textViews_[1]->setText("Checking for Update");
    textViews_[2]->setText("Downloading Update");
    textViews_[3]->setText("Installing Update");

    // Initialize the popup views for messages
    popupView_ = std::make_shared<BoxView>(display, 5);
    popupView_->fillColor = display.bg;
    popupView_->setHidden(true);
    popupTextView_ = std::make_shared<MultiLineTextView>(display, 10);
    popupTextView_->alignCenter = true;
    popupTextView_->dynamicWidth = true;
    popupTextView_->setHidden(true);
}

auto FirmwareUpdateScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setSublayoutFrame(
        Rect::fromSize(size), LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;

            for (int i = 0; i < 4; i++) {
                auto textView = textViews_[i];
                auto checkmarkView = checkmarkViews_[i];
                auto progressBarView = progressBarViews_[i];
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
            context.setViewSizeInRect(*popupTextView_, {150, 150}, Rect::fromSize(size),
                                      LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
        context.setViewFrame(*popupView_, popupTextRect.inset(-10));
    }

    context.setViewFrame(cancelView_, Rect::fromSize(size).inset(10),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);

    return {size.width, size.height};
}

void FirmwareUpdateScreen::drawInRect(Rect rect) {
    for (const auto &textView : textViews_) {
        (*textView).draw();
    }
    for (const auto &progressBarView : progressBarViews_) {
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

void FirmwareUpdateScreen::showPopup(const std::string &message) {
    popupTextView_->setText(message);
    popupView_->setHidden(false);
    popupTextView_->setHidden(false);
    setNeedsLayout();
    nextUpdate_ = DisplaySystem::UpdateType::PARTIAL;
}

void FirmwareUpdateScreen::cancel() {
    CancelFlag::cancel(cancel_);
    firmwareDownloader_.cancel();
    telemetry_.logMetric(TelemetryField::METRIC_FIRMWARE_UPDATE, "update-cancelled");
    WiFi::getInstance().disconnectWiFi();
    showPopup("Update cancelled!\n\nRestarting...");
    render(display_);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    back();
}

void FirmwareUpdateScreen::render(DisplaySystem &display) {
    int xPadding = 42;
    int yPadding = 10;
    setFrame(Rect::fromSize({display.width(), display.height()}).inset(xPadding, yPadding));

    // Re-render based on state
    switch (state_.state) {
    case FirmwareDownloadState::Idle:
        break;
    case FirmwareDownloadState::ConnectingWifi:
        progressBarViews_[0]->setProgress(0);
        break;
    case FirmwareDownloadState::CheckingForLatest:
        progressBarViews_[0]->setProgress(100);
        progressBarViews_[1]->setProgress(50);
        checkmarkViews_[0]->setHidden(false);
        break;
    case FirmwareDownloadState::Downloading: {
        std::string newText =
            StringFormat("Downloading Update %s", firmwareDownloader_.versionString.c_str());
        if (textViews_[2]->getText() != newText) {
            textViews_[2]->setText(newText);
            nextUpdate_ = DisplaySystem::UpdateType::PARTIAL;
        }
        textViews_[2]->setText(
            StringFormat("Downloading Update %s", firmwareDownloader_.versionString.c_str()));
        progressBarViews_[0]->setProgress(100);
        progressBarViews_[1]->setProgress(100);
        checkmarkViews_[0]->setHidden(false);
        checkmarkViews_[1]->setHidden(false);
        progressBarViews_[2]->setProgress(state_.percentage);
        break;
    }
    case FirmwareDownloadState::AlreadyDownloaded:
    case FirmwareDownloadState::Downloaded:
        progressBarViews_[0]->setProgress(100);
        progressBarViews_[1]->setProgress(100);
        progressBarViews_[2]->setProgress(100);
        checkmarkViews_[0]->setHidden(false);
        checkmarkViews_[1]->setHidden(false);
        checkmarkViews_[2]->setHidden(false);
        progressBarViews_[3]->setProgress(state_.percentage);
        break;
    case FirmwareDownloadState::Installing: {
        std::string newText =
            StringFormat("Installing Update %s", firmwareDownloader_.versionString.c_str());
        if (textViews_[3]->getText() != newText) {
            textViews_[3]->setText(newText);
            nextUpdate_ = DisplaySystem::UpdateType::PARTIAL;
        }
        progressBarViews_[0]->setProgress(100);
        progressBarViews_[1]->setProgress(100);
        progressBarViews_[2]->setProgress(100);
        progressBarViews_[3]->setProgress(50);
        checkmarkViews_[0]->setHidden(false);
        checkmarkViews_[1]->setHidden(false);
        checkmarkViews_[2]->setHidden(false);
        checkmarkViews_[3]->setHidden(false);
        break;
    }
    case FirmwareDownloadState::Installed:
    case FirmwareDownloadState::AlreadyUpToDate: {
        progressBarViews_[0]->setProgress(100);
        progressBarViews_[1]->setProgress(100);
        progressBarViews_[2]->setProgress(100);
        progressBarViews_[3]->setProgress(100);
        checkmarkViews_[0]->setHidden(false);
        checkmarkViews_[1]->setHidden(false);
        checkmarkViews_[2]->setHidden(false);
        checkmarkViews_[3]->setHidden(false);
        if (state_.state == FirmwareDownloadState::AlreadyUpToDate) {
            showPopup(StringFormat("Already up to date\n\n(version: %s)", SemanticVersionString()));
        } else {
            showPopup(StringFormat("Updated to %s\n\nRestarting...",
                                   firmwareDownloader_.versionString.c_str()));
        }
        break;
    }
    case FirmwareDownloadState::Failure:
        showPopup(StringFormat("Update failed:\n\n%s...", state_.message.c_str()));
        break;
    }

    draw();

    display.updateDisplay(nextUpdate_);

    nextUpdate_ = DisplaySystem::UpdateType::FAST_INDICATOR;
}

void FirmwareUpdateScreen::processCommand(RemoteCommand command) {
    if (command == RemoteCommand::ENTER) {
        cancel();
    }
}

void FirmwareUpdateScreen::fail(const std::string &message) {
    log_w("Firmware update failed: %s", message.c_str());
    WiFi::getInstance().disconnectWiFi();

    this->telemetry_.logMetric(TelemetryField::METRIC_FIRMWARE_UPDATE, "update-failed",
                               {{"message", message}});
    state_.state = FirmwareDownloadState::Failure;
    state_.message = message;
    render(display_);
    vTaskDelay(pdMS_TO_TICKS(3000));
    back();
}

void FirmwareUpdateScreen::begin() {
    if (!WiFi::getInstance().hasWiFiCredentials()) {
        fail("Go to Settings > WiFi Setup in the Sol app.");
        return;
    }

#if USE_BLE
    // Stop BLE advertising and disconnect all connections to avoid interference.
    BLEManager::getInstance()->stopAdvertising();
    auto bleManager = BLEManager::getInstance();
    bleManager->disconnectAllClients();
    auto resumeBLE = [bleManager] { bleManager->startAdvertisingIfNecessary(); };
#else
    auto resumeBLE = [] {};
#endif

    state_.state = FirmwareDownloadState::ConnectingWifi;
    render(display_);

    if (!WiFi::getInstance().connectWiFiFromSettings()) {
        fail("Failed to connect to WiFi.");
        resumeBLE();
        return;
    }

    auto rl = std::make_shared<RateLimiter>(1, 20, 1);
    cancel_ = std::make_shared<CancelFlag>();
    firmwareDownloader_.downloadFirmware([this, rl, resumeBLE](const FirmwareDownloadState &state) {
        if (CancelFlag::isCancelled(cancel_)) {
            return;
        }

        PowerManager::getInstance().feedInactivityTimer();
        if (state.state == FirmwareDownloadState::Failure) {
            telemetry_.logMetric(TelemetryField::METRIC_FIRMWARE_UPDATE, "update-failed");
        }
        if (state.state == FirmwareDownloadState::AlreadyUpToDate ||
            state.state == FirmwareDownloadState::Failure ||
            state.state == FirmwareDownloadState::Installed) {
            WiFi::getInstance().disconnectWiFi();
        }
        // Skip updates if the state is the same and the percentage isn't different (or if not
        // enough time has elapsed)
        if (state.state == state_.state) {
            if (!rl->shouldExecute(state.percentage)) {
                return;
            }
        } else {
            rl->resetPercentage();
        }

        DispatchGetUiQueue()->dispatch([this, state, resumeBLE]() {
            updateState(state); // Update the internal state with the new state.
            render(display_);   // Render the UI with the updated state.

            if (state.state == FirmwareDownloadState::AlreadyUpToDate ||
                state.state == FirmwareDownloadState::Failure) {
                vTaskDelay(pdMS_TO_TICKS(3000));
                resumeBLE();
                back();
            } else if (state.state == FirmwareDownloadState::Installed) {
                vTaskDelay(pdMS_TO_TICKS(3000));
                PowerManager::getInstance().reboot();
            }
        });
    });
}
