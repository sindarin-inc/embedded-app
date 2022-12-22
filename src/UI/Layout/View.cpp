#include "View.hpp"

#include "LayoutMath.hpp"

LayoutContext::LayoutContext(View &view)
    : layout_([&view](LayoutContext &layoutContext, Size size) { return view.layout(size); }),
      frame_(Rect::zero()) {}

auto LayoutContext::setViewSizeInRect(View &view, const Size &size, const Rect &inRect, int options)
    -> Rect {
    auto frame = FrameForView(view, size, inRect, options);
    log_d("Calculated frame for view %p: %d %d %d %d", &view, frame.origin.x, frame.origin.y,
          frame.size.width, frame.size.height);
    if (!sizing_) {
        log_d("Setting for view %p: %d %d %d %d", &view, frame.origin.x, frame.origin.y,
              frame.size.width, frame.size.height);
        view.setFrame(frame);
    }
    return frame;
}

auto LayoutContext::setViewFrame(View &view, const Rect &frame, int options) -> Rect {
    return setViewSizeInRect(view, frame.size, frame, options);
}

auto LayoutContext::setSublayoutFrame(const Rect &frame, int options, const LayoutLambda &layout)
    -> Rect {
    // Create a new layout context for this sublayout if one doesn't exist already
    auto it = sublayouts_.find(&layout);
    if (it == sublayouts_.end()) {
        sublayouts_.emplace(&layout, std::make_unique<SublayoutContext>(layout));
        log_d("Creating new sublayout context for %p", &layout);
    }
    auto sublayoutContext = sublayouts_[&layout].get();
    sublayoutContext->sizing_ = sizing_;

    // Run the layout and keep track of all the views that were laid out so we can adjust their
    // offset later
    auto size = (*sublayoutContext).layoutIfNeeded(frame.size, sizing_);
    log_d("setSublayoutFrame: %d %d %d %d pointer = %p size = %d %d sizing = %d %d", frame.origin.x,
          frame.origin.y, frame.size.width, frame.size.height, &layout, size.width, size.height,
          sizing_, sublayoutContext->sizing_);

    // Now, if we're not sizing, go through and adjust the frames of all the views that were
    // laid out
    if (!sizing_) {
        (*sublayoutContext).adjustViewFramesForOffset(frame.origin);
    }

    return {frame.origin, size};
}

auto LayoutContext::layoutIfNeeded(const Size &size, bool sizing) -> Size {
    if (size == cachedSize_ && ((!needsSizing_ && sizing) || (!needsLayout_ && !sizing))) {
        return cachedLayoutSize_;
    }

    sizing_ = sizing;
    cachedSize_ = size;
    auto layoutSize = layout_(*this, size);
    cachedLayoutSize_ = layoutSize;
    if (!sizing_) {
        needsLayout_ = false;
    }
    needsSizing_ = false;
    sizing_ = false;
    return layoutSize;
}

void SublayoutContext::adjustViewFramesForOffset(const Point &offset) {
    for (auto view : sublayoutViews_) {
        log_d("Adjusting view %p in frame %d %d %d %d offset %d %d", view, view->frame().origin.x,
              view->frame().origin.y, view->frame().size.width, view->frame().size.height, offset.x,
              offset.y);
        view->setFrame(OffsetFrame(view->frame(), offset));
    }
}
