#include "TestWebRTCScreen.hpp"

#include <cstdio>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>

#include "WebRTC/OpenAI.hpp"
#include "WebRTC/ToolCalling.hpp"
#include "sindarin-debug.h"

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
    OpenAI openai;

    // Register demonstration tool calls ----------------------------------
    auto &tool = openai.toolCalling();

    // Light tool ---------------------------------------------------------
    auto redAttr = tool.createIntAttribute("red", "Red value in the range of 0-255", true,
                                           [](attribute_t *attr, const char *, const char *) {
        printf("Red set to %d\n", attr->i_value);
        return 0;
    });
    auto greenAttr = tool.createIntAttribute("green", "Green value in the range of 0-255", true,
                                             [](attribute_t *attr, const char *, const char *) {
        printf("Green set to %d\n", attr->i_value);
        return 0;
    });
    auto blueAttr = tool.createIntAttribute("blue", "Blue value in the range of 0-255", true,
                                            [](attribute_t *attr, const char *, const char *) {
        printf("Blue set to %d\n", attr->i_value);
        return 0;
    });
    auto colorParent = tool.createParentAttribute(
        "LightColor", "Set light color of red, green and blue", {redAttr, greenAttr, blueAttr});

    auto stateAttr = tool.createBoolAttribute("LightState", "New light state (true or false)", true,
                                              [](attribute_t *attr, const char *, const char *) {
        printf("Light set to %s\n", attr->b_state ? "ON" : "OFF");
        return 0;
    });
    tool.registerTool("SetLightState", "Changes the state of the light", {stateAttr, colorParent});

    // Speaker volume tool ------------------------------------------------
    auto volumeAttr = tool.createIntAttribute("volume", "Speaker volume range 0-100", true,
                                              [](attribute_t *attr, const char *, const char *) {
        printf("Volume set to %d\n", attr->i_value);
        return 0;
    });
    tool.registerTool("SetVolume", "Changes speaker volume", {volumeAttr});

    // Door tool ----------------------------------------------------------
    auto doorAttr = tool.createBoolAttribute("open", "Open or close the door", true,
                                             [](attribute_t *attr, const char *, const char *) {
        printf("Door is %s\n", attr->b_state ? "Opened" : "Closed");
        return 0;
    });
    tool.registerTool("OpenDoor", "Toggle the door state to open or close", {doorAttr});

    // Now init and start WebRTC -----------------------------------------
    openai.init();
    openai.start();
    // openai.sendText("Hello, how are you?");
    // openai.stop();
    vTaskDelete(nullptr);
}

void TestWebRTCScreen::launchWebRtcTasks() {
    // Launch begin_webrtc in its own task – it contains an infinite loop.
    xTaskCreatePinnedToCore(beginWebRtcTask, "webrtc_begin", 8 * 1024, nullptr, 5, nullptr, 1);
}

// MARK: - Public API ---------------------------------------------------------
void TestWebRTCScreen::begin() {
    SHOW_HEAP();
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

    SHOW_HEAP();

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