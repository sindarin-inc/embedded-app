#pragma once

#include "LayoutMath.hpp"
#include "Types.hpp"

class View;

// The Layout object:
// * Encapsulates the complexity of figuring out when a layout is needed
class LayoutContext {
public:
    LayoutContext(View &view) : view_(view), frame_(Rect::zero()) {}
    void setNeedsLayout() {
        needsLayout_ = true;
        needsSizing_ = true;
        cachedSize_ = Size::zero();
    }

    // Getter and setter for the frame
    [[nodiscard]] auto frame() const -> const Rect & { return frame_; }
    void setFrame(const Rect &frame) {
        // If we're changing the size, we'll need to trigger a re-layout
        if (frame_.size != frame.size) {
            setNeedsLayout();
        }
        frame_ = frame;
    }

    // This is the root layout/sizing method. It accepts a view size (which may change if using
    // SizeToFit* options) as well as a rect in which to draw the view. When using the Align*
    // options, the view's size will be positioned inside inRect in order to determine the view's
    // frame.
    auto setViewSizeInRect(View &view, const Size &size, const Rect &inRect,
                           int options = LayoutOptionsNone) -> Rect;

    // This is a convenience method that calls setViewSizeInRect with size=frame.size. This is often
    // used when using the SizeToFit* options to allow the view to determine its own
    // size within the provided rect
    auto setViewFrame(View &view, const Rect &frame, int options = LayoutOptionsNone) -> Rect;

    // This runs the layout if necessary, or returns the cached size if layout is not
    // necessary
    auto layoutIfNeeded(const Size &size, bool sizing) -> Size;

protected:
    // Backreference to the View so that we can call its `layout` method. An alternate
    // implementation could be to have the layout implementation stored in a lambda on the
    // LayoutContext
    View &view_;
    // TODO: Can we combine frame_ and cachedSize_?
    // TODO: I'm still on the fence: should frame_ belong to the view or the LayoutContext?
    Rect frame_;

    bool sizing_;
    bool needsLayout_{true};
    bool needsSizing_{true};
    Size cachedSize_;
    Size cachedLayoutSize_;
};

class View {
public:
    // Constructor
    View() : layoutContext_(*this) {}

    // Getter and setter for the frame
    // TODO: Why does this need to be on view? Because LayoutContext is private and not exposed
    // directly
    [[nodiscard]] auto frame() const -> const Rect & { return layoutContext_.frame(); }
    void setFrame(const Rect &frame) { layoutContext_.setFrame(frame); }

    // View subclasses should implement this method to layout any elements and return their
    // desired size. This will be called by LayoutContext and should not be called directly.
    virtual auto layout(Size size) -> Size = 0;

    // Virtual method for drawing the contents of the view inside a provided rect
    // TODO: Would we want to force a layout within rect here?
    // Maybe views always call the superclass and we provide an implementation here that sets frame
    // and lays out if needed
    virtual void drawInRect(Rect rect) {
        layoutContext_.setFrame(rect);
        layoutIfNeeded();
    };

    void draw() { drawInRect(layoutContext_.frame()); }
    // TODO: Maybe we just need to pass in the parent's origin here instead of the whole view
    void drawInParent(const View &view) {
        // TODO: But what about the parent's parent etc?
        drawInRect(OffsetFrame(layoutContext_.frame(), view.frame().origin));
    }

    // Set flags indicating layout is needed
    void setNeedsLayout() { layoutContext_.setNeedsLayout(); }

    // Should be called sometime before drawing
    void layoutIfNeeded() { layoutContext_.layoutIfNeeded(layoutContext_.frame().size, false); }

    // TODO: Only used from SizeThatFitsForView and probably should never be called externally
    [[nodiscard]] auto sizeThatFits(const Size &size) const -> Size {
        // Dangerous cast to non-const LayoutContext because we are guaranteeing that when
        // sizing==true we are not going to modify anything
        return const_cast<LayoutContext &>(layoutContext_).layoutIfNeeded(size, true);
    }

protected:
    LayoutContext layoutContext_;
};
