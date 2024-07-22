#pragma once

#include "EinkView.hpp"

// A view that draws an icon
class IconView : public EinkView {
public:
    IconView(DisplaySystem &display, const unsigned char *iconBits, int width, int height,
             int scale = 1)
        : EinkView(display), iconBits_(iconBits), width_(width), height_(height), scale_(scale) {}
    ~IconView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {
        return {width_ * scale_, height_ * scale_};
    }

    void setIcon(const unsigned char *iconBits, int width, int height) {
        iconBits_ = iconBits;
        width_ = width;
        height_ = height;
        setNeedsLayout();
    }

    void drawInRect(Rect rect) override {
#ifdef SIM_LAYOUT_DEBUG
        // For debugging
        display_.drawRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                          display_.fg);
#endif
        if (scale_ == 1) {
            display_.drawXBitmap(rect.origin.x, rect.origin.y, iconBits_, width_, height_,
                                 display_.fg);
        } else {
            display_.drawScaledXBitmap(rect.origin.x, rect.origin.y, iconBits_, width_, height_,
                                       display_.fg, scale_);
        }
    }

protected:
    const unsigned char *iconBits_;
    int width_;
    int height_;
    int scale_;
};
