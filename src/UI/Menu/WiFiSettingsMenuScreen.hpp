#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/Screen.hpp"

class WiFiSettingsMenuScreen : public Screen, public EinkView {
public:
    WiFiSettingsMenuScreen(DisplaySystem &display);

    void addMenuItem(const std::shared_ptr<NewMenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    // Must be called any time the wifi settings could have changed.
    void loadWifiSettings();

    void render(DisplaySystem &display) override;

private:
    NewMenuView menuView_;
    ArrowLabelView backLabelView_;
    SingleLineTextView configuredAccessView_;
    MultiLineTextView configuredAccessPointsView_;

    void moveSelection(int delta);
};
