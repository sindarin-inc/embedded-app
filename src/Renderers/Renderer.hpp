#ifndef RENDERER_H
#define RENDERER_H

#include "../Displays/DisplaySystem.hpp"

class Renderer {
public:
    Renderer(DisplaySystem *displayInput);

    void renderMaxBox();
    void renderMaxBoxWithText(const String &text);

    void renderBox(int xStart, int yStart, int xEnd, int yEnd);

    auto getPreviousPlace() -> unsigned long;

    auto getDisplay() -> DisplaySystem * { return display_; }

    auto getPixelWidth() -> int { return maxPixelsX_; }
    auto getPixelHeight() -> int { return maxPixelsY_; }

    int localXOffset = 0;
    int localYOffset = 0;

protected:
    DisplaySystem *display_;

    void computeMinMax();
    void computeMaxLines();

    int xMin_ = 0;
    int xMax_ = 0;
    int yMin_ = 0;
    int yMax_ = 0;
    int maxPixelsX_ = 0;
    int maxPixelsY_ = 0;

    int maxLines_ = 0;
};

#endif