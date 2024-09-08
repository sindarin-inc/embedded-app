#pragma once

#include "Renderers/Renderer.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"

class RendererView : public EinkView {
public:
    RendererView(DisplaySystem &display, Renderer &renderer);

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    bool dottedBox = false;
    uint16_t padding = 3;

private:
    Renderer &renderer_;
    BoxView dottedBoxView_;
};
