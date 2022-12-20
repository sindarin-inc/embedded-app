#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Types.hpp"

class View;

// The Layout object:
// * Encapsulates the complexity of figuring out when a layout is needed
class LayoutContext {
public:
    LayoutContext(View &view) : view_(view), needsLayout_(true), needsSizing_(true) {}
    void setNeedsLayout() {
        needsLayout_ = true;
        needsSizing_ = true;
        cachedSize_ = Size::zero();
    }

    auto setViewSizeInRect(View &view, const Size &size, const Rect &inRect, int options) -> Rect;

    auto setViewFrame(View &view, const Rect &frame, int options) -> Rect;

    // This runs the layout if necessary, or returns the cached size if layout is not
    // necessary
    auto layoutIfNeeded(const Size &size, bool sizing) -> Size;

protected:
    auto setViewFrameIfNecessary(View &view, const Rect &frame) -> Rect;

protected:
    View &view_;

    bool sizing_;
    bool needsLayout_;
    bool needsSizing_;
    Size cachedSize_;
    Size cachedLayoutSize_;
};

class View {
public:
    // Constructor
    View(DisplaySystem &display) : display_(display), frame_(Rect::zero()), layoutContext_(*this) {}

    // Getter and setter for the frame
    const Rect &frame() const { return frame_; }
    void setFrame(const Rect &frame) { frame_ = frame; }

    // View subclasses should implement this method to layout any elements and return their
    // desired size
    virtual auto layout(Size size) -> Size = 0;

    // Virtual method for drawing the contents of the view
    virtual void draw() = 0;

    // Set flags indicating layout is needed
    void setNeedsLayout() { layoutContext_.setNeedsLayout(); }
    void layoutIfNeeded() { layoutContext_.layoutIfNeeded(frame_.size, false); }
    auto sizeThatFits(const Size &size) const -> Size {
        // Dangerous cast to non-const LayoutContext because we are guaranteeing that when
        // sizing==true we are not going to modify anything
        return const_cast<LayoutContext &>(layoutContext_).layoutIfNeeded(size, true);
    }

protected:
    DisplaySystem &display_;
    Rect frame_;
    LayoutContext layoutContext_;
};
