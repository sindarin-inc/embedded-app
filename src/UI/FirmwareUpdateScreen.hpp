#pragma once

#include <string>
#include <utility>

#include "Communications/FirmwareDownload.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/ProgressBarView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Screen.hpp"

class FirmwareUpdateScreen : public Screen, public EinkView {
public:
    FirmwareUpdateScreen(DisplaySystem &display, Telemetry &telemetry, PreferencesStore &prefs);

    void begin();

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    Telemetry &telemetry_;
    std::shared_ptr<CancelFlag> cancel_;
    FirmwareDownloader firmwareDownloader_;
    FirmwareDownloadState state_;
    DisplaySystem::UpdateType nextUpdate_ = DisplaySystem::UpdateType::FULL;

    // UI
    std::array<std::shared_ptr<SingleLineTextView>, 4> textViews_;
    std::array<std::shared_ptr<IconView>, 4> checkmarkViews_;
    std::array<std::shared_ptr<ProgressBarView>, 4> progressBarViews_;
    std::shared_ptr<BoxView> popupView_;
    std::shared_ptr<MultiLineTextView> popupTextView_;
    SingleLineTextView cancelView_;

    void cancel();
    void showPopup(const std::string &message);
    void fail(const std::string &message);
    void updateState(const FirmwareDownloadState &state) {
        state_ = state;
        setNeedsRerender();
    }
};
