#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

#include "LayoutMath.hpp"
#include "Types.hpp"

class View;
class LayoutContext;

// Lambda that contains layout operations for a view or sublayout
typedef std::function<Size(LayoutContext &, Size)> LayoutLambda;

// The LayoutContext object:
// * Encapsulates the complexity of figuring out when a layout is needed
// * Caches the size of a layout and determines when cache should be invalidated
// * Holds onto any sublayouts defined in the layout lambda
// * Provides helper methods for laying out sublayouts and views
class LayoutContext {
public:
    LayoutContext(LayoutLambda layout) : layout_(std::move(layout)), frame_(Rect::zero()) {}
    // Convenience method that creates the layout lambda automatically from the view's layout method
    LayoutContext(View &view);

    // Getter for the frame in the screen's coordinate space. This is determined by walking back
    // through each parent and getting the origin of each layout all the way back to the root (which
    // has no parent)
    [[nodiscard]] auto screenFrame() const -> Rect {
        Point offset = {0, 0};
        if (parentLayout_.has_value()) {
            offset = parentLayout_.value()->screenFrame().origin;
        }
        return frame_.offset(offset);
    }

    // Getter for the frame in the local coordinate space. This is the frame that was set in the
    // parent layout's coordinate system
    [[nodiscard]] auto frame() const -> Rect { return frame_; }

    // Sets the frame in local coordinate space (relative to the parent layout's coordinate system)
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
    auto setViewFrame(View &view, const Rect &frame, int options = LayoutOptionsNone) -> Rect {
        return setViewSizeInRect(view, frame.size, frame, options);
    }

    // Similar to setViewSizeInRect except it accepts a layout lambda instead of a view. The layout
    // lambda can operate as if it were a view with its own layout method.
    auto setSublayoutSizeInRect(const Size &size, const Rect &inRect, int options,
                                const LayoutLambda &layout) -> Rect;

    // Similar to setViewFrame except it accepts a layout lambda instead of a view. The layout
    // lambda can operate as if it were a view with its own layout method.
    auto setSublayoutFrame(const Rect &frame, int options, const LayoutLambda &layout) -> Rect {
        return setSublayoutSizeInRect(frame.size, frame, options, layout);
    }

    // Invalidates any caching on the layout. This should be called when the content is changed in
    // such a way that the calculated size may change.
    void setNeedsLayout() {
        needsLayout_ = true;
        needsSizing_ = true;
        cachedSize_ = Size::zero();
        cachedLayoutSize_ = Size::zero();
    }

    // This runs the layout if necessary, or returns the cached size if layout is not
    // necessary
    auto layoutIfNeeded(const Size &size, bool sizing) -> Size;

protected:
    // The root layout method. This is called by setViewSizeInRect and setSublayoutSizeInRect to do
    // all the actual layout work.
    auto setLayoutSizeInRect(LayoutContext &layoutContext, const Size &size, const Rect &inRect,
                             int options) -> Rect;

    auto setLayoutFrame(LayoutContext &layoutContext, const Rect &frame, int options) -> Rect {
        return setLayoutSizeInRect(layoutContext, frame.size, frame, options);
    }

    // Layout implementation with all the code that represents the view's layout
    LayoutLambda layout_;

    // The frame of the layout in the parent layout's coordinate space
    Rect frame_{{0, 0}, {0, 0}};

    // Whether this layout is currently sizing (doesn't change frames) or whether it's running for
    // real
    bool sizing_;

    // Whether this layout needs to be run again to position its views.
    bool needsLayout_{true};

    // Whether this layout needs to be run again to determine its size.
    bool needsSizing_{true};

    // Cached input size. Used as the cache key to determine if we need to re-layout to determine
    // the size.
    Size cachedSize_{0, 0};

    // Cached output size. This is the size that is returned by the layout method when passing in
    // cachedSize_.
    Size cachedLayoutSize_{0, 0};

    // Pointer to the parent layout that contains this layout. Used for figuring out the
    // global-coordinatine-space frame for this layout
    std::optional<LayoutContext *> parentLayout_;

    // Layouts are counted by the order of their invocation.
    // WARNING: Sublayouts should always get called and get called in the same order inside layout
    // lambdas.
    int sublayoutIndex_ = 0;

    // Collection of inline sublayouts. Only used when the layout lambda creates a sublayout.
    std::map<int, std::shared_ptr<LayoutContext>> sublayouts_;
};
