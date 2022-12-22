#include "View.hpp"

#include "LayoutMath.hpp"

auto LayoutContext::setViewSizeInRect(View &view, const Size &size, const Rect &inRect, int options)
    -> Rect {
    auto frame = FrameForView(view, size, inRect, options);
    if (!sizing_) {
        view.setFrame(frame);
    }
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
