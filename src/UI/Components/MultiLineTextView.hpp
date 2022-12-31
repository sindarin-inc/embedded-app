#pragma once

#include "EinkView.hpp"
#include "Renderers/Renderer.hpp"

// A view that draws a line of text
class MultiLineTextView : public EinkView {
public:
    MultiLineTextView(DisplaySystem &display, std::string text)
        : EinkView(display), text_(std::move(text)), renderer_(display) {
        auto stream = new StdStringStream(text_);
        renderer_.setStream(stream);
    }
    ~MultiLineTextView() override = default;

    void setMaxWidth(uint32_t width) { maxWidth_ = width; }

    auto layout(LayoutContext &context, Size size) -> Size override {
        uint16_t h;
        unsigned int numLines = 0;
        DisplayRect oldBounding = display_.getBoundingBox();
        DisplayRect newBounding = {0, 0, maxWidth_, oldBounding.height};
        display_.setBoundingBox(newBounding);

        // Fake render to set the number of lines rendered
        renderer_.renderPage(0, 100);
        numLines = renderer_.getNumRenderedLines();
        h = numLines * display_.getFont()->yAdvance + (numLines - 1) * display_.getLineSpacing();
        display_.setBoundingBox(oldBounding);
        return {(int)maxWidth_, h};
    }

    void drawInRect(Rect rect) override {
        DisplayRect oldBounding = display_.getBoundingBox();
        // The renderer pads 2 pixels so we adjust accordingly here.
        DisplayRect newBounding = {rect.origin.x + oldBounding.x, rect.origin.y + oldBounding.y,
                                   maxWidth_, oldBounding.height - (rect.origin.y - oldBounding.y)};
        display_.setBoundingBox(newBounding);
        renderer_.renderPage(0, 0);
        display_.setBoundingBox(oldBounding);
    }

protected:
    std::string text_;
    uint32_t maxWidth_ = 255;
    Renderer renderer_;
};