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
    uint8_t unusedDarkPicturePercent = 0;
    if (!autoPilotMode_) {
        renderer_.renderFirstPage(0, true);
    } else {
        std::tie(nextPlace_, unusedDarkPicturePercent) =
            renderer_.renderPage(currentPlace_, numSkipLines_, true);
        if (numSkipLines_ >= renderer_.getNumRenderedLines()) {
            numSkipLines_ = 0;
            currentPlace_ = nextPlace_;
        } else {
            numSkipLines_++;
        }
        // If the line is blank, we want to skip it. Also, skip multiple newlines in a row.
        while ((renderer_.getLengthOfLine(numSkipLines_) == 0) &&
               numSkipLines_ < renderer_.getNumRenderedLines()) {
            numSkipLines_++;
        }

        if (renderer_.atEndOfFile() && numSkipLines_ >= renderer_.getNumRenderedLines()) {
            // If we've rendered the whole stream, reset to the beginning.
            numSkipLines_ = 0;
            currentPlace_ = nullptr;
            nextPlace_ = nullptr;
        }
    }
}