#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/RendererView.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/RootUI.hpp"
#include "UI/Screen.hpp"

class LetsFocusWelcomeMenuScreen : public Screen, public EinkView {
public:
    LetsFocusWelcomeMenuScreen(DisplaySystem &display);

    void addMenuItem(const std::shared_ptr<NewMenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;
    void render(DisplaySystem &display) override;

private:
    NewMenuView menuView_;
    Renderer renderer_;
    RendererView rendererView_;
    ArrowView leftArrowView_;
    ArrowView rightArrowView_;
    uint8_t textSelection_ = 0;

    std::vector<std::tuple<std::string, Inset>> texts_ = {
        {"Turn each lens like binoculars until this text is in focus.", {50, 40, 85, 65}},
        {"For some readers, the distance between their eyes impacts focus. The knob on top allows "
         "you to adjust the eye spacing. Line the lenses up so each one is directly in front of "
         "each eye.",
         {35, 21, 58, 22}},
    };

    void setText();
    void moveSelection(int delta);
};
