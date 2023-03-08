#pragma once

#include "LayoutContext.hpp"
#include "LayoutMath.hpp"
#include "Types.hpp"

class View {
public:
    // Constructor
    View() : layoutContext_(*this) {}
    virtual ~View() = default;

    // Getter and setter for the frame
    void setFrame(const Rect &frame) { layoutContext_.setFrame(frame); }

    // Virtual method for drawing the contents of the view inside a provided rect. Typically not
    // called directly. Use `draw()` instead.
    virtual void drawInRect(Rect rect) = 0;

    // Draw the view in its current screen-coordinate frame
    void draw() {
        layoutIfNeeded();
        drawInRect(layoutContext_.screenFrame());
    }

    // Set flags indicating layout is needed. Call this when a view's content changes enough that
    // the sizing may change
    void setNeedsLayout() { layoutContext_.setNeedsLayout(); }

    // Should be called sometime before drawing
    void layoutIfNeeded() { layoutContext_.layoutIfNeeded(layoutContext_.frame().size, false); }

private:
    // View subclasses should implement this method to layout any elements and return their
    // desired size. This will be called by LayoutContext and should not be called directly.
    virtual auto layout(LayoutContext &context, Size size) -> Size = 0;

    LayoutContext layoutContext_;
    friend class LayoutContext;
};
