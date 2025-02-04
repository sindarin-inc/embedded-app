#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"

class WiFiSettingsMenuScreen : public Screen, public EinkView {
public:
    WiFiSettingsMenuScreen(DisplaySystem &display);

    void addMenuItem(const std::shared_ptr<MenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    // Must be called any time the wifi settings could have changed.
    void loadWifiSettings();

    void render(DisplaySystem &display) override;

private:
    MenuView menuView_;
    ArrowLabelView backLabelView_;
    SingleLineTextView configuredAccessView_;
    MultiLineTextView configuredAccessPointsView_;

    void moveSelection(int delta);
};
