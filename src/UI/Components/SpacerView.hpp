#pragma once

#include <utility>

#include "EinkView.hpp"
#include "Misc/Style.hpp"
#include "UI/Fonts/TextTruncation.hpp"

// A view that draws a line of text
class SpacerView : public EinkView {
public:
    SpacerView(DisplaySystem &display, int height) : EinkView(display), height_(height) {}

    auto layout(LayoutContext &context, Size size) -> Size override {
        return {size.width, height_};
    }

    auto getHeight() const -> int { return height_; }

    void drawInRect(Rect rect) override {
        // Spacer doesn't draw anything
    }

private:
    int height_;
};
