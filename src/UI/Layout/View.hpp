#pragma once

#include "LayoutContext.hpp"
#include "LayoutMath.hpp"
#include "Types.hpp"

class View {
public:
    // Constructor
    View() : layoutContext_(*this) {}
    virtual ~View() = default;

    // Copy constructor needs to recreate layoutContext_ since it captures `this` in a lambda
    // TODO: This isn't awesome. We should consider either removing the copy constructor or making
    // it so we always pass the view into the layout
    View(const View &other) : layoutContext_(*this), hidden_(other.hidden_) {}

    // Getter and setter for the frame
    void setFrame(const SolRect &frame) { layoutContext_.setFrame(frame); }

    // Virtual method for drawing the contents of the view inside a provided rect. Typically not
    // called directly. Use `draw()` instead.
    virtual void drawInRect(SolRect rect) = 0;

    // Draw the view in its current screen-coordinate frame
    virtual void draw() {
        if (hidden_) {
            return;
        }
        layoutIfNeeded();
        drawInRect(layoutContext_.screenFrame());
    }

    virtual auto debugName() -> std::string {
        // Since IDF uses -fno-rtti by default (no run-time type info) we don't have the name of the
        // current subclass. If you really want to debug on device, you could always override this
        // method in the View you're debugging.
#if SIMULATOR
        return typeid(*this).name();
#else
        return "View";
#endif
    }

    // Acccessors for hidden_ field
    [[nodiscard]] auto isHidden() const -> bool { return hidden_; }
    void setHidden(bool hidden) { hidden_ = hidden; }

    // Set flags indicating layout is needed. Call this when a view's content changes enough that
    // the sizing may change
    void setNeedsLayout() { layoutContext_.setNeedsLayout(); }

    // Should be called sometime before drawing
    void layoutIfNeeded() { layoutContext_.layoutIfNeeded(layoutContext_.frame().size, false); }

protected:
    LayoutContext layoutContext_;
    friend class LayoutContext;

private:
    // View subclasses should implement this method to layout any elements and return their
    // desired size. This will be called by LayoutContext and should not be called directly.
    virtual auto layout(LayoutContext &context, SolSize size) -> SolSize = 0;

    bool hidden_ = false;
};
