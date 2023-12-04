#pragma once

#include "Misc/CancelFlag.hpp"
#include "Misc/Telemetry.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Bitmaps/CheckMark.xbm"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/ProgressBarView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Screen.hpp"

class ContentSyncScreen : public Screen, public EinkView {
public:
    ContentSyncScreen(DisplaySystem &display, LocalStore &localStore, PreferencesStore &prefs,
                      Telemetry &telemetry);

    void begin();

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    LocalStore &localStore_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    ContentSyncState state_;
    DisplaySystem::UpdateType nextUpdate_ = DisplaySystem::UpdateType::FULL;
    std::shared_ptr<CancelFlag> cancel_;

    // UI
    std::array<std::shared_ptr<SingleLineTextView>, 4> textViews_;
    std::array<std::shared_ptr<IconView>, 4> checkmarkViews_;
    std::array<std::shared_ptr<ProgressBarView>, 4> progressBarsViews_;
    std::shared_ptr<BoxView> popupView_;
    std::shared_ptr<MultiLineTextView> popupTextView_;
    SingleLineTextView cancelView_;

    void cancel();
    void showPopup(const std::string &message);
    void fail(const std::string &message);
    void updateState(const ContentSyncState &state) {
        state_ = state;
        setNeedsRerender();
    }
};
