#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/RendererView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"

class ReadingSettingsMenuScreen : public Screen, public EinkView {
public:
    ReadingSettingsMenuScreen(DisplaySystem &display, const std::string &backTitle,
                              PreferencesStore &prefs);

    auto setSelectedIndex(size_t index) -> bool;

    void addMenuItem(const std::shared_ptr<MenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

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

    // When they press back, we need to stop the animation timer and reset the renderer
    void leaveMenu() {
        rendererView_.resetRenderer();
        xTimerStop(DisplaySystem::animationTimer, 0);
    }

private:
    MenuView menuView_;
    ArrowLabelView backLabelView_;
    Renderer renderer_;

    RendererView rendererView_;

    PreferencesStore &prefs_;

    void moveSelection(int delta);
};
