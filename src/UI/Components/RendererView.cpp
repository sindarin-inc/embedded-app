#include "RendererView.hpp"

RendererView::RendererView(DisplaySystem &display, Renderer &renderer)
    : EinkView(display), renderer_(renderer), dottedBoxView_(display) {
    dottedBoxView_.cornerRadius = 3;
    dottedBoxView_.fillColor = display.bg;
    dottedBoxView_.strokeColor = display.fg;
    dottedBoxView_.dotted = true;
}

auto RendererView::layout(LayoutContext &context, SolSize size) -> SolSize {

    auto outerBox = SolRect::fromSize(size);
    context.setViewFrame(dottedBoxView_, outerBox);
    return size;
}

void RendererView::resetTimer() {
    auto totalLength = renderer_.getTotalLengthOfLines();
    nextInterval_ =
        totalLength != 0 ? pageSpeed_ * renderer_.getLengthOfLine(numSkipLines_) / totalLength : 0;
    if (nextInterval_ == 0) {
        nextInterval_ = 1000;
    }
    xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
}

void RendererView::drawInRect(SolRect rect) {
    if (dottedBox) {
        dottedBoxView_.draw();
    }
    auto textBox = rect.inset(padding);
    renderer_.setRect(DisplayRect::fromRect(textBox));
    uint8_t unusedDarkPicturePercent = 0;
    if (!autoPilotMode_) {
        renderer_.renderFirstPage(0, true);
    } else {
        std::tie(nextPlace_, unusedDarkPicturePercent) =
            renderer_.renderPage(currentPlace_, numSkipLines_, true);

        resetTimer();

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
            if (autoPilotWraparound_) {
                // If we've rendered the whole stream, reset to the beginning if that flag is set..
                numSkipLines_ = 0;
                currentPlace_ = nullptr;
                nextPlace_ = nullptr;
            } else {
                // If we've rendered the whole stream, stop the animation.
                xTimerStop(DisplaySystem::animationTimer, 0);
            }
        }
    }
}