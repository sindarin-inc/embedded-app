#pragma once

#include "LayoutContext.hpp"
#include "LayoutMath.hpp"
#include "Types.hpp"
#include "sindarin-debug.h"

class View {
public:
    // Constructor
    View() : layoutContext_(*this) {}

    // Getter and setter for the frame
    void setFrame(const Rect &frame) { layoutContext_.setFrame(frame); }

    // Virtual method for drawing the contents of the view inside a provided rect. Typically not
    // called directly.
    virtual void drawInRect(Rect rect) = 0;

    void draw() { drawInRect(layoutContext_.globalFrame()); }

    // Set flags indicating layout is needed
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
