#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/RendererView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/LineSpacingScreen.hpp"
#include "UI/Menu/DarkModeMenuItemCheckable.hpp"
#include "UI/Menu/GenericSettingsMenuScreen.hpp"
#include "UI/Menu/MenuItemCheckable.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/RootUI.hpp"
#include "UI/Screen.hpp"

class ExperimentalMenuScreen : public Screen, public EinkView {
public:
    using PushScreenFunc = std::function<void(const std::shared_ptr<Screen> &)>;

    ExperimentalMenuScreen(DisplaySystem &display, const std::string &backTitle,
                           PreferencesStore &prefs, Telemetry &telemetry,
                           PushScreenFunc pushScreen);

    auto setSelectedIndex(size_t index) -> bool;

    void addMenuItem(const std::shared_ptr<MenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void resetFont();

    void render(DisplaySystem &display) override;

    void regenerateMenuItems() {
        size_t lastIndex = menuView_.getSelectedIndex();
        menuView_.resetMenuItems();
        populateMenuItems();
        menuView_.setSelectedIndex(lastIndex);
    }

    void populateMenuItems();

private:
    MenuView menuView_;
    ArrowLabelView backLabelView_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    PushScreenFunc pushScreen_;

    void moveSelection(int delta);
};
