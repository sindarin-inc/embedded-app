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
#include "UI/Screen.hpp"

class ReadingSettingsMenuScreen : public Screen, public EinkView {
public:
    using PushScreenFunc = std::function<void(const std::shared_ptr<Screen> &)>;
    using PopScreenFunc = std::function<void()>;

    ReadingSettingsMenuScreen(DisplaySystem &display, const std::string &backTitle,
                              PreferencesStore &prefs, Telemetry &telemetry,
                              PushScreenFunc pushScreen, PopScreenFunc popScreen);

    auto setSelectedIndex(size_t index) -> bool;

    void addMenuItem(const std::shared_ptr<MenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void resetFont();

    void render(DisplaySystem &display) override;

    void turnOnAutoPilot() {
        rendererView_.setAutopilot(true);
        setFirstLoad();
    }

    void turnOffAutoPilot() {
        rendererView_.resetRenderer();
        rendererView_.setAutopilot(false);
        setFirstLoad();
    }

    void setRendererLineSpacingFromPrefs() {
        renderer_.setLineSpacing(prefs_.lineSpacing.get().value_or(0));
    }

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
    RendererView rendererView_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    PushScreenFunc pushScreen_;
    PopScreenFunc popScreen_;

    void moveSelection(int delta);
};
