#pragma once

#include "EinkView.hpp"

// A view that draws an icon
class IconView : public EinkView {
public:
    IconView(DisplaySystem &display, const unsigned char *iconBits, int width, int height)
        : EinkView(display), iconBits_(iconBits), width_(width), height_(height) {}
    ~IconView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override { return {width_, height_}; }

    void setIcon(const unsigned char *iconBits, int width, int height) {
        iconBits_ = iconBits;
        width_ = width;
        height_ = height;
        setNeedsLayout();
    }

    void drawInRect(Rect rect) override {
        display_.drawXBitmap(rect.origin.x, rect.origin.y, iconBits_, width_, height_, display_.fg);
    }

protected:
    const unsigned char *iconBits_;
    int width_;
    int height_;
};