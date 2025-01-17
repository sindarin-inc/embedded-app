#pragma once

#include <vector>

#include "Misc/Platform.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Screen.hpp"

class DebugAboutScreen : public Screen, public EinkView {
public:
    DebugAboutScreen(DisplaySystem &display);

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;

    std::vector<std::shared_ptr<SingleLineTextView>> textViews_;
    SingleLineTextView returnToMenuView_;
};
