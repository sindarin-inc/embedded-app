#pragma once

#include <Renderer/Renderer.hpp>

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/RendererView.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/RootUI.hpp"
#include "UI/Screen.hpp"

class FirstBookWelcomeMenuScreen : public Screen, public EinkView {
public:
    FirstBookWelcomeMenuScreen(DisplaySystem &display);

    void addMenuItem(const std::shared_ptr<MenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;
    void render(DisplaySystem &display) override;

private:
    MenuView menuView_;
    Renderer renderer_;
    RendererView rendererView_;

    void moveSelection(int delta);
};
