#include "LayoutContext.hpp"

#include "Misc/StringUtil.hpp"
#include "View.hpp"

LayoutContext::LayoutContext(View &view)
    : layout_([this, &view](LayoutContext &layoutContext, Size size) {
          return view.layout(*this, size);
      }),
      frame_(Rect::zero()) {}

auto LayoutContext::setLayoutSizeInRect(LayoutContext &layoutContext, const Size &size,
                                        const Rect &inRect, int options) -> Rect {
    layoutContext.parentLayout_ = this;

    auto frame = FrameForLayout(layoutContext, size, inRect, options);

    if (!sizing_) {
        layoutContext.setFrame(frame);
    }
    return frame;
}

auto LayoutContext::setViewSizeInRect(View &view, const Size &size, const Rect &inRect, int options)
    -> Rect {
    return setLayoutSizeInRect(view.layoutContext_, size, inRect, options);
}

auto LayoutContext::setSublayoutSizeInRect(const Size &size, const Rect &inRect, int options,
                                           const LayoutLambda &layout) -> Rect {
    // Create a new layout context for this sublayout if one doesn't exist already
    auto it = sublayouts_.find(sublayoutIndex_);
    if (it == sublayouts_.end()) {
        sublayouts_.emplace(sublayoutIndex_, std::make_shared<LayoutContext>(layout));
    }
    auto sublayoutContext = sublayouts_[sublayoutIndex_].get();

    sublayoutIndex_++;

    Rect frame = setLayoutSizeInRect(*sublayoutContext, size, inRect, options);
    // Since there's no `draw` method for a sublayout (which is where we'd normally call
    // `layoutIfNeeded`), we go ahead and run the real layout here if we're not sizing.
    if (!sizing_) {
        (*sublayoutContext).layoutIfNeeded(frame.size, false);
    }

    return frame;
}

auto LayoutContext::layoutIfNeeded(const Size &size, bool sizing) -> Size {
    if (size == cachedSize_ && ((!needsSizing_ && sizing) || (!needsLayout_ && !sizing))) {
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
