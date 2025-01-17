#include "LayoutContext.hpp"

#include "Misc/StringUtil.hpp"
#include "View.hpp"

LayoutContext::LayoutContext(View &view)
    : view_(&view), layout_([this, &view](LayoutContext &layoutContext, SolSize size) {
          return view.layout(*this, size);
      }),
      frame_(SolRect::zero()) {}

auto LayoutContext::debugName() -> std::string {
    if (view_ == nullptr) {
        if (parentLayout_.has_value()) {
            return StringFormat("Sublayout of %s", parentLayout_.value()->debugName().c_str());
        } else {
            return "Sublayout";
        }
    } else {
        return view_->debugName();
    }
}

auto LayoutContext::setLayoutSizeInRect(LayoutContext &layoutContext, const SolSize &size,
                                        const SolRect &inRect, int options) -> SolRect {
    layoutContext.parentLayout_ = this;

// Since we don't have run-time type information on the device right now, these logs are only really
// useful in the sim
#if SIMULATOR
    // Make sure we're not trying to both align center and align right
    if ((options & LayoutOptionsAlignCenterHorizontal) != 0 &&
        (options & LayoutOptionsAlignRight) != 0) {
        log_w("Aligning %s inside %s with conflicting alignment options "
              "(LayoutOptionsAlignCenter/Horizontal and LayoutOptionsAlignRight) "
              "(this is probably a bug)",
              layoutContext.debugName().c_str(), debugName().c_str());
    }

    // Make sure we're not trying to align center and align bottom
    if ((options & LayoutOptionsAlignCenterVertical) != 0 &&
        (options & LayoutOptionsAlignBottom) != 0) {
        log_w("Aligning %s inside %s with conflicting alignment options "
              "(LayoutOptionsAlignCenter/Vertical and LayoutOptionsAlignBottom) "
              "(this is probably a bug)",
              layoutContext.debugName().c_str(), debugName().c_str());
    }

    // Raise a warning if we're aligning but not sizing (i.e. we are trying to align two equal sized
    // rects)
    if (size == inRect.size && ((options & LayoutOptionsSizeToFit) == 0) &&
        ((options &
          (LayoutOptionsAlignCenter | LayoutOptionsAlignRight | LayoutOptionsAlignBottom)) != 0)) {
        log_w("Aligning %s inside %s in an equally sized Rect without sizing (this is probably a "
              "bug)",
              layoutContext.debugName().c_str(), debugName().c_str());
    }
#endif

    auto frame = FrameForLayout(layoutContext, size, inRect, options);

    if (!sizing_) {
        layoutContext.setFrame(frame);
    }
    return frame;
}

auto LayoutContext::setViewSizeInRect(View &view, const SolSize &size, const SolRect &inRect,
                                      int options) -> SolRect {
    return setLayoutSizeInRect(view.layoutContext_, size, inRect, options);
}

auto LayoutContext::setSublayoutSizeInRect(const SolSize &size, const SolRect &inRect, int options,
                                           const LayoutLambda &layout) -> SolRect {
    // Create a new layout context for this sublayout if one doesn't exist already
    auto it = sublayouts_.find(sublayoutIndex_);
    if (it == sublayouts_.end()) {
        sublayouts_.emplace(sublayoutIndex_, std::make_shared<LayoutContext>(layout));
    }
    auto sublayoutContext = sublayouts_[sublayoutIndex_].get();

    sublayoutIndex_++;

    SolRect frame = setLayoutSizeInRect(*sublayoutContext, size, inRect, options);
    // Since there's no `draw` method for a sublayout (which is where we'd normally call
    // `layoutIfNeeded`), we go ahead and run the real layout here if we're not sizing.
    if (!sizing_) {
        (*sublayoutContext).layoutIfNeeded(frame.size, false);
    }

    return frame;
}

auto LayoutContext::layoutIfNeeded(const SolSize &size, bool sizing) -> SolSize {
    if ((size == cachedSize_ || size == cachedLayoutSize_) &&
        ((!needsSizing_ && sizing) || (!needsLayout_ && !sizing))) {
        return cachedLayoutSize_;
    }

    sizing_ = sizing;
    cachedSize_ = size;
    sublayoutIndex_ = 0;
    auto layoutSize = layout_(*this, size);
    cachedLayoutSize_ = layoutSize;
    if (!sizing_) {
        needsLayout_ = false;
    }
    needsSizing_ = false;
    sizing_ = false;
    return layoutSize;
}
