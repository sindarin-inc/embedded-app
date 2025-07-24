#include "TestWebRTCScreen.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ----------------------------------------------------------------------------
TestWebRTCScreen::TestWebRTCScreen(DisplaySystem &display, PreferencesStore &prefs)
    : EinkView(display), prefs_(prefs) {
    statusView_ = std::make_shared<MultiLineTextView>(display, /*maxLines=*/6);
    statusView_->alignCenter = true;
    statusView_->dynamicWidth = true;
    updateStatus("Initializing…");
}

// MARK: - Private helpers ----------------------------------------------------
void TestWebRTCScreen::updateStatus(const std::string &msg) {
    statusMessage_ = msg;
    statusView_->setText(statusMessage_);
    setNeedsLayout();
    setNeedsRerender();
    nextUpdate_ = DisplaySystem::UpdateType::PARTIAL;
}

static void beginWebRtcTask(void * /*param*/) {
    begin_webrtc();
    vTaskDelete(nullptr);
}

void TestWebRTCScreen::launchWebRtcTasks() {
    // Launch begin_webrtc in its own task – it contains an infinite loop.
    xTaskCreatePinnedToCore(beginWebRtcTask, "webrtc_begin", 8 * 1024, nullptr, 5, nullptr, 1);
}

// MARK: - Public API ---------------------------------------------------------
void TestWebRTCScreen::begin() {
    if (!WiFi::getInstance().hasWiFiCredentials()) {
        updateStatus("No Wi-Fi credentials.\nUse the Sol app → Settings → Wi-Fi setup.");
        render(display_);
        vTaskDelay(pdMS_TO_TICKS(3000));
        back();
        return;
    }

    // Attempt Wi-Fi connection (no BLE fallback here – this is a quick test).
    updateStatus("Connecting to Wi-Fi…");
    render(display_);

    if (!WiFi::getInstance().connectWiFiFromSettings()) {
        updateStatus("Failed to connect to Wi-Fi.");
        render(display_);
        vTaskDelay(pdMS_TO_TICKS(3000));
        back();
        return;
    }

    updateStatus("Wi-Fi connected.\nStarting WebRTC…");
    render(display_);

    launchWebRtcTasks();

    updateStatus("WebRTC started!\nPress ← (BACK) to exit.");
    render(display_);
}

// MARK: - Screen overrides ---------------------------------------------------
void TestWebRTCScreen::render(DisplaySystem &display) {
    // Constrain content to default inset box
    setFrame(defaultFrameRect());
    draw();
    display.updateDisplay(nextUpdate_);
    nextUpdate_ = DisplaySystem::UpdateType::FAST_INDICATOR;
    clearNeedsRerender();
}

void TestWebRTCScreen::processCommand(RemoteCommand command) {
    if (command == RemoteCommand::BACK) {
        back();
        return;
    }
}

// MARK: - EinkView overrides -------------------------------------------------
auto TestWebRTCScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    context.setViewSizeInRect(*statusView_, {size.width, size.height}, SolRect::fromSize(size),
                              LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
    return {size.width, size.height};
}

void TestWebRTCScreen::drawInRect(SolRect /*rect*/) { statusView_->draw(); }