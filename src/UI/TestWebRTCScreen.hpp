#pragma once

#include <memory>
#include <string>

#include "Communications/WiFi.hpp"
#include "Displays/DisplaySystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Screen.hpp"

/*!
 * A very small utility screen that connects to Wi-Fi and then kicks off the
 * WebRTC demo (begin_webrtc() followed by start_webrtc()).  All user-interface
 * is deliberately minimal – we only show a single status label so we can
 * observe progress over the air while bringing the feature up.
 */
class TestWebRTCScreen : public Screen, public EinkView {
public:
    TestWebRTCScreen(DisplaySystem &display, PreferencesStore &prefs);

    /* Kick off the flow: connect to Wi-Fi, launch WebRTC helper threads, update UI. */
    void begin();

    // Screen overrides
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected: // EinkView overrides
    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

private:
    // Helpers
    void updateStatus(const std::string &msg);
    void launchWebRtcTasks();

    PreferencesStore &prefs_;
    std::shared_ptr<MultiLineTextView> statusView_;
    std::string statusMessage_;
    DisplaySystem::UpdateType nextUpdate_ = DisplaySystem::UpdateType::FULL;
};