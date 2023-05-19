#pragma once

#include "EinkView.hpp"
#include "Misc/PBMImage.hpp"

// A view that draws an icon
class IconView : public EinkView {
public:
    IconView(DisplaySystem &display, const unsigned char *iconBits, int width, int height)
        : EinkView(display), iconBits_(iconBits), width_(width), height_(height), size_(0) {}
    ~IconView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override { return {width_, height_}; }

    void setIcon(const unsigned char *iconBits, int width, int height) {
        iconBits_ = iconBits;
        width_ = width;
        height_ = height;
        setNeedsLayout();
    }

    void setPBMIcon(const unsigned char *iconBits, int size) {
        iconBits_ = iconBits;
        size_ = size;
        BufferStream stream(iconBits_, size);
        if (!ParsePbmHeader(stream, width_, height_)) {
            log_e("failed to parse pbm header");
            return;
        }
        setNeedsLayout();
    }

    void drawInRect(Rect rect) override {
        if (size_ > 0) {
            BufferStream stream(iconBits_, size_);
            display_.drawPBM(rect.origin.x, rect.origin.y, stream, display_.fg);
        } else {
            display_.drawXBitmap(rect.origin.x, rect.origin.y, iconBits_, width_, height_,
                                 display_.fg);
        }
    }

protected:
    const unsigned char *iconBits_;
    int width_;
    int height_;
    int size_;
};