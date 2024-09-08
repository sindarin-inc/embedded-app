#include "RendererView.hpp"

RendererView::RendererView(DisplaySystem &display, Renderer &renderer)
    : EinkView(display), renderer_(renderer), dottedBoxView_(display) {
    dottedBoxView_.cornerRadius = 3;
    dottedBoxView_.fillColor = display.bg;
    dottedBoxView_.strokeColor = display.fg;
    dottedBoxView_.dotted = true;
}

auto RendererView::layout(LayoutContext &context, Size size) -> Size {

    auto outerBox = Rect::fromSize(size);
    context.setViewFrame(dottedBoxView_, outerBox);
    return size;
}

void RendererView::drawInRect(Rect rect) {
    if (dottedBox) {
        dottedBoxView_.draw();
    }
    auto textBox = rect.inset(padding);
    renderer_.setRect(DisplayRect(
        {static_cast<uint16_t>(textBox.left()), static_cast<uint16_t>(textBox.top()),
         static_cast<uint16_t>(textBox.size.width), static_cast<uint16_t>(textBox.size.height)}));
    renderer_.renderFirstPage(0, true);
}