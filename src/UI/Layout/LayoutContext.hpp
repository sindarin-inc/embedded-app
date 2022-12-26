#pragma once

#include <map>
#include <unordered_set>

#include "LayoutMath.hpp"
#include "Types.hpp"
#include "sindarin-debug.h"

class View;
class LayoutContext;

// Lambda that contains layout operations for a view or sublayout
typedef std::function<Size(LayoutContext &, Size)> LayoutLambda;

// The Layout object:
// * Encapsulates the complexity of figuring out when a layout is needed
// * Caches the size of a layout and determines when cache should be invalidated
// * Provides helper methods for laying out sublayouts and views
class LayoutContext {
public:
    LayoutContext(const LayoutLambda &layout) : layout_(std::move(layout)), frame_(Rect::zero()) {}
    // Convenience method that creates the layout lambda automatically from the view's layout method
    LayoutContext(View &view);

    // Getter for the frame in the global coordinate space
    [[nodiscard]] auto globalFrame() const -> Rect {
        Point offset = {0, 0};
        if (parentLayout_.has_value()) {
            offset = parentLayout_.value()->globalFrame().origin;
        }
        return OffsetFrame(frame_, offset);
    }

    // Sets the frame in local coordinate space
    void setFrame(const Rect &frame) {
        // If we're changing the size, we'll need to trigger a re-layout
        // TODO: Do we need this?
        // TODO: Force a relayout on subviews too? Does that need to happen via the layout block?
        if (frame_.size != frame.size) {
            setNeedsLayout();
        }
        frame_ = frame;
    }

    // This is the root layout/sizing method. It accepts a size (which may change if using
    // SizeToFit* options) as well as a rect in which to draw the view. When using the Align*
    // options, the size will be positioned inside inRect in order to determine the
    // frame.
    auto setViewSizeInRect(View &view, const Size &size, const Rect &inRect,
                           int options = LayoutOptionsNone) -> Rect;

    // This is a convenience method that calls setLayoutSizeInRect with size=frame.size. This is
    // often used when using the SizeToFit* options to allow the layout to determine its own size
    // within the provided rect
    auto setViewFrame(View &view, const Rect &frame, int options = LayoutOptionsNone) -> Rect;

    // Similar to setViewSizeInRect except it accepts a layout lambda instead of a view. The layout
    // lambda can operate as if it were a view with its own layout method.
    auto setSublayoutSizeInRect(const Size &size, const Rect &inRect, int options,
                                const LayoutLambda &layout) -> Rect;

    auto setSublayoutFrame(const Rect &frame, int options, const LayoutLambda &layout) -> Rect;

    void setNeedsLayout() {
        needsLayout_ = true;
        needsSizing_ = true;
        cachedSize_ = Size::zero();
    }

    // This runs the layout if necessary, or returns the cached size if layout is not
    // necessary
    auto layoutIfNeeded(const Size &size, bool sizing) -> Size;
    bool sizing_;

protected:
    auto setLayoutSizeInRect(LayoutContext &layoutContext, const Size &size, const Rect &inRect,
                             int options) -> Rect;

    auto setLayoutFrame(LayoutContext &layoutContext, const Rect &frame, int options) -> Rect;

    // Backreference to the View so that we can call its `layout` method. An alternate
    // implementation could be to have the layout implementation stored in a lambda on the
    // LayoutContext
    LayoutLambda layout_;

    // TODO: Can we combine frame_ and cachedSize_?
    // TODO: I'm still on the fence: should frame_ belong to the view or the LayoutContext?
    Rect frame_;

    bool needsLayout_{true};
    bool needsSizing_{true};
    Size cachedSize_;
    Size cachedLayoutSize_;

    // Pointer to the parent layout that contains this layout. Used for figuring out the
    // global-coordinatine-space frame for this layout
    std::optional<LayoutContext *> parentLayout_;

    // Collection of inline nested layouts
    std::map<const LayoutLambda *, std::unique_ptr<LayoutContext>> sublayouts_;
};
