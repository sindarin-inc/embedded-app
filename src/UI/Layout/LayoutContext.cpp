#include "LayoutContext.hpp"

#include "View.hpp"
#include "sindarin-debug.h"

LayoutContext::LayoutContext(View &view)
    : layout_([this, &view](LayoutContext &layoutContext, Size size) {
          return view.layout(*this, size);
      }),
      frame_(Rect::zero()) {}

auto LayoutContext::setLayoutSizeInRect(LayoutContext &layoutContext, const Size &size,
                                        const Rect &inRect, int options) -> Rect {
    log_d("setLayoutSizeInRect: this=%p size %d %d inRect %d %d %d %d sizing %d", this, size.width,
          size.height, inRect.origin.x, inRect.origin.y, inRect.size.width, inRect.size.height,
          sizing_);

    layoutContext.parentLayout_ = this;

    auto frame = FrameForLayout(layoutContext, size, inRect, options);
    log_d("Calculated frame for layoutContext %p: %d %d %d %d", &layoutContext, frame.origin.x,
          frame.origin.y, frame.size.width, frame.size.height);
    // if (!sizing_) {
    log_d("Setting for layout %p: %d %d %d %d", &layoutContext, frame.origin.x, frame.origin.y,
          frame.size.width, frame.size.height);
    layoutContext.setFrame(frame);
    // }
    return frame;
}

auto LayoutContext::setLayoutFrame(LayoutContext &layoutContext, const Rect &frame, int options)
    -> Rect {
    return setLayoutSizeInRect(layoutContext, frame.size, frame, options);
}

auto LayoutContext::setViewSizeInRect(View &view, const Size &size, const Rect &inRect, int options)
    -> Rect {
    return setLayoutSizeInRect(view.layoutContext_, size, inRect, options);
}

auto LayoutContext::setViewFrame(View &view, const Rect &frame, int options) -> Rect {
    return setViewSizeInRect(view, frame.size, frame, options);
}

// Sublayouts of sublayouts

auto LayoutContext::setSublayoutSizeInRect(const Size &size, const Rect &inRect, int options,
                                           const LayoutLambda &layout) -> Rect {
    log_d("setSublayoutSizeInRect: this=%p size %d %d inRect %d %d %d %d sizing %d", this,
          size.width, size.height, inRect.origin.x, inRect.origin.y, inRect.size.width,
          inRect.size.height, sizing_);
    // Create a new layout context for this sublayout if one doesn't exist already
    auto it = sublayouts_.find(&layout);
    if (it == sublayouts_.end()) {
        sublayouts_.emplace(&layout, std::make_unique<LayoutContext>(layout));
        log_d("Creating new sublayout context for %p", &layout);
    }
    auto sublayoutContext = sublayouts_[&layout].get();

    // TODO: How do you invalidate sublayout layouts since there's not the normal view-subview
    // relationship?

    sublayoutContext->sizing_ = sizing_;

    Rect frame = setLayoutSizeInRect(*sublayoutContext, size, inRect, options);

    log_d("Calculated frame for sublayout %p: %d %d %d %d inrect %d %d %d %d", &layout,
          frame.origin.x, frame.origin.y, frame.size.width, frame.size.height, inRect.origin.x,
          inRect.origin.y, inRect.size.width, inRect.size.height);

    return frame;
}

auto LayoutContext::setSublayoutFrame(const Rect &frame, int options, const LayoutLambda &layout)
    -> Rect {
    return setSublayoutSizeInRect(frame.size, frame, options, layout);
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
        log_d("Setting layout to false for %p", this);
        needsLayout_ = false;
    }
    needsSizing_ = false;
    log_d("Done layout for %p: %d %d", this, layoutSize.width, layoutSize.height);
    sizing_ = false;
    return layoutSize;
}

// Big open questions:
// * In our use case, does sizing separately from laying out have value?

// * At what point do we store the frame in the local coordinate space? What point do we do the
// global?
//    I think this could be at draw time (what i had previously), at frame calculation time (what I
//    sorta tried to do for sublayouts but needed everywhere), or at frame reading time (what i'm
//    doing now)

// * How do you invalidate sublayout layouts since there's not the normal view-subview relationship?

// * Is caching working? Is it even worth it?