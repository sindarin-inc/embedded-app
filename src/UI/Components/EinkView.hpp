#pragma once

#include "Displays/DisplaySystem.hpp"
#include "UI/Layout/View.hpp"

class EinkView : public View {
public:
    EinkView(DisplaySystem &display) : display_(display) {}

    void draw() override {
        View::draw();
        if (drawDebugBox) {
            auto rect = layoutContext_.screenFrame();
            display_.drawRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                              display_.fg);
        }
    }

    auto defaultFrameRect() -> Rect {
        auto defaultInset = display_.defaultUIInsets();
        return Rect::fromSize({display_.width(), display_.height()})
            .inset(defaultInset.left, defaultInset.right, defaultInset.top, defaultInset.bottom);
    }
#ifdef SIM_LAYOUT_DEBUG
    bool drawDebugBox = true;
#else
    bool drawDebugBox = false;
#endif

protected:
    DisplaySystem &display_;
};
