#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/RendererView.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/RootUI.hpp"
#include "UI/Screen.hpp"

class FirstBookWelcomeMenuScreen : public Screen, public EinkView {
public:
    FirstBookWelcomeMenuScreen(DisplaySystem &display);

    void addMenuItem(const std::shared_ptr<NewMenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;
    void render(DisplaySystem &display) override;

private:
    NewMenuView menuView_;
    Renderer renderer_;
    RendererView rendererView_;

    void moveSelection(int delta);
};
