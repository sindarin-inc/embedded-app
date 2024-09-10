#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/Screen.hpp"

class WiFiSettingsMenuScreen : public Screen, public EinkView {
public:
    WiFiSettingsMenuScreen(DisplaySystem &display, PreferencesStore &prefs);

    void addMenuItem(const std::shared_ptr<NewMenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void setFirstLoad() { firstLoad_ = true; }

    // Must be called any time the wifi settings could have changed.
    void loadWifiSettings();

    void showTemporaryMessage(DisplaySystem &display, const std::string &message,
                              int duration = 2000, bool rerenderAfter = false);

    void render(DisplaySystem &display) override;

private:
    PreferencesStore &prefs_;
    NewMenuView menuView_;
    ArrowLabelView backLabelView_;
    SingleLineTextView wifiEnabledView_;
    SingleLineTextView bleEnabledView_;
    SingleLineTextView configuredAccessView_;
    MultiLineTextView configuredAccessPointsView_;

    bool firstLoad_{true};

    void moveSelection(int delta);
};
