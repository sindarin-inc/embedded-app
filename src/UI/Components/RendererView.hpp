#pragma once

#include "Renderers/Renderer.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"

class RendererView : public EinkView {
public:
    RendererView(DisplaySystem &display, Renderer &renderer);

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void setAutopilot(bool autoPilotMode) { autoPilotMode_ = autoPilotMode; }

    auto autoPilotMode() const -> bool { return autoPilotMode_; }

    auto setAutoPilotWraparound() { autoPilotWraparound_ = true; }

    void resetRenderer() {
        numSkipLines_ = 0;
        currentPlace_ = nullptr;
        nextPlace_ = nullptr;
    }

    auto newPage() const -> bool { return autoPilotMode_ ? (numSkipLines_ == 1) : true; }

    bool dottedBox = false;
    uint16_t padding = 3;

private:
    Renderer &renderer_;
    BoxView dottedBoxView_;
    // For autopilot mode
    bool autoPilotMode_ = false;
    bool autoPilotWraparound_ = false;
    uint16_t numSkipLines_ = 0;
    std::shared_ptr<Place> currentPlace_ = nullptr;
    std::shared_ptr<Place> nextPlace_ = nullptr;

    void resetTimer();

    unsigned long nextInterval_ = 0;
    int pageSpeed_ = 10000;
};
