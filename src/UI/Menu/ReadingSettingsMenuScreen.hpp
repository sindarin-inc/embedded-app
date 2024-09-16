#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/RendererView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/Screen.hpp"

class ReadingSettingsMenuScreen : public Screen, public EinkView {
public:
    ReadingSettingsMenuScreen(DisplaySystem &display, const std::string &backTitle);

    auto setSelectedIndex(size_t index) -> bool;

    void addMenuItem(const std::shared_ptr<NewMenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void setFirstLoad() { firstLoad_ = true; }

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

    // When they press back, we need to stop the animation timer and reset the renderer
    void leaveMenu() {
        rendererView_.resetRenderer();
        xTimerStop(DisplaySystem::animationTimer, 0);
    }

private:
    NewMenuView menuView_;
    ArrowLabelView backLabelView_;
    Renderer renderer_;

    RendererView rendererView_;

    bool firstLoad_{true};

    void moveSelection(int delta);
};
