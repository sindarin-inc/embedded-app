#pragma once

#include <map>
#include <memory>
#include <unordered_set>
#include <utility>

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
    LayoutContext(LayoutLambda layout) : layout_(std::move(layout)), frame_(Rect::zero()) {}
    // Convenience method that creates the layout lambda automatically from the view's layout method
    LayoutContext(View &view);

    // Getter for the frame in the global coordinate space. This is determined by walking back
    // through each parent and getting the origin of each layout all the way back to the root (which
    // has no parent)
    [[nodiscard]] auto globalFrame() const -> Rect {
        Point offset = {0, 0};
        if (parentLayout_.has_value()) {
            offset = parentLayout_.value()->globalFrame().origin;
        }
        return frame_.offset(offset);
    }

    // Getter for the frame in the local coordinate space. This is the frame that was set in the
    // parent layout's coordinate system
    [[nodiscard]] auto frame() const -> Rect { return frame_; }

    // Sets the frame in local coordinate space
    void setFrame(const Rect &frame) {
        // If we're changing the size, we'll need to trigger a re-layout
        if (frame_.size != frame.size) {
            needsLayout_ = true;
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

protected:
    auto setLayoutSizeInRect(LayoutContext &layoutContext, const Size &size, const Rect &inRect,
                             int options) -> Rect;

    auto setLayoutFrame(LayoutContext &layoutContext, const Rect &frame, int options) -> Rect;

    // Backreference to the View so that we can call its `layout` method. An alternate
    // implementation could be to have the layout implementation stored in a lambda on the
    // LayoutContext
    LayoutLambda layout_;

    Rect frame_;

    bool sizing_;
    bool needsLayout_{true};
    bool needsSizing_{true};
    // TODO: Test that caching is working
    Size cachedSize_;
    Size cachedLayoutSize_;

    // Pointer to the parent layout that contains this layout. Used for figuring out the
    // global-coordinatine-space frame for this layout
    std::optional<LayoutContext *> parentLayout_;

    // Collection of inline nested layouts
    std::map<const LayoutLambda *, std::unique_ptr<LayoutContext>> sublayouts_;
};
