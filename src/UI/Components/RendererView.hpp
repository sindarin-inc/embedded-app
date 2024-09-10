#pragma once

#include "Renderers/Renderer.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"

class RendererView : public EinkView {
public:
    RendererView(DisplaySystem &display, Renderer &renderer);

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void setAutopilot(bool autoPilotMode) { autoPilotMode_ = autoPilotMode; }

    void resetRenderer() {
        numSkipLines_ = 0;
        currentPlace_ = nullptr;
        nextPlace_ = nullptr;
    }

    auto getNumSkipLines() -> uint16_t { return numSkipLines_; }

    bool dottedBox = false;
    uint16_t padding = 3;

private:
    Renderer &renderer_;
    BoxView dottedBoxView_;
    // For autopilot mode
    bool autoPilotMode_ = false;
    uint16_t numSkipLines_ = 0;
    std::shared_ptr<Place> currentPlace_ = nullptr;
    std::shared_ptr<Place> nextPlace_ = nullptr;
};
