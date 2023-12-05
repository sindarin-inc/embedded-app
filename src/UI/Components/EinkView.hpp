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

    bool drawDebugBox = false;

protected:
    DisplaySystem &display_;
};
