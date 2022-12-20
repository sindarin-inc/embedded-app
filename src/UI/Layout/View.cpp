#include "View.hpp"

#include "LayoutMath.hpp"

auto LayoutContext::setViewSizeInRect(View &view, const Size &size, const Rect &inRect, int options)
    -> Rect {
    // We could consider offsetting by the view_.frame.origin here so that everything
    // knows where to draw itself on the screen. But I'm not sure that view_.frame will
    // already be set at this point. Maybe better to do the offsetting in the draw
    // method
    auto frame = FrameForView(view, size, inRect, options);
    setViewFrameIfNecessary(view, frame);
    return frame;
}

auto LayoutContext::setViewFrame(View &view, const Rect &frame, int options) -> Rect {
    return setViewSizeInRect(view, frame.size, frame, options);
}

auto LayoutContext::layoutIfNeeded(const Size &size, bool sizing) -> Size {
    if (size == cachedSize_ && ((!needsSizing_ && sizing) || (!needsLayout_ && !sizing))) {
        return cachedLayoutSize_;
    }

    sizing_ = sizing;
    cachedSize_ = size;
    auto layoutSize = view_.layout(size);
    cachedLayoutSize_ = layoutSize;
    if (!sizing_) {
        needsLayout_ = false;
    }
    needsSizing_ = false;
    sizing_ = false;
    return layoutSize;
}

auto LayoutContext::setViewFrameIfNecessary(View &view, const Rect &frame) -> Rect {
    if (!sizing_) {
        view.setFrame(frame);
        // Since we are applying the frame, the subview will need to re-layout
        view.setNeedsLayout();
    }
    return frame;
}
