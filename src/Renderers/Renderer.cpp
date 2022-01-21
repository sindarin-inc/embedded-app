#include "Renderer.hpp"

#include <utility>

#include "sindarin-debug.h"

Renderer::Renderer(DisplaySystem *displayInput) {
    display_ = displayInput;
    computeMinMax();
}

void Renderer::computeMinMax() {
    int midpointX;
    int midpointY;
    // pixels per mm
    float pixelPitchX;
    float pixelPitchY;

    log_d("Computing Min and Max pixels for Renderer");

    midpointX = display_->width() / 2;
    midpointY = display_->height() / 2;

    log_d("midpoint = (%d, %d)", midpointX, midpointY);

    pixelPitchX = ((float)display_->width()) / display_->getWidthInMm();
    pixelPitchY = ((float)display_->height()) / display_->getHeightInMm();

    log_d("pixel pitch = (%d, %d)", pixelPitchX, pixelPitchY);

    // rounding down is ok here and what we want.
    maxPixelsX_ = (int)(display_->getFocusWidth() * pixelPitchX);
    maxPixelsY_ = (int)(display_->getFocusHeight() * pixelPitchY);

    log_d("max pixels = (%d, %d)", maxPixelsX_, maxPixelsY_);

    xMin_ = midpointX - maxPixelsX_ / 2;
    xMax_ = midpointX + maxPixelsX_ / 2;
    yMin_ = midpointY - maxPixelsY_ / 2;
    yMax_ = midpointY + maxPixelsY_ / 2;

    log_d("min = (%d, %d) max = (%d, %d)", xMin_, yMin_, xMax_, yMax_);
}

// Ask the display for the line height so we can set the max lines based
// on our calcualtion.

void Renderer::computeMaxLines() {}

void Renderer::renderMaxBox() {
    display_->drawBox(xMin_ + localXOffset, yMin_ + localYOffset, xMax_ + localXOffset,
                      yMax_ + localYOffset);
}

void Renderer::renderMaxBoxWithText(const String &text) {
    display_->drawTextInBox(text, xMin_ + localXOffset, yMin_ + localYOffset, xMax_ + localXOffset,
                            yMax_ + localYOffset);
}

void Renderer::renderBox(int xStart, int yStart, int xEnd, int yEnd) {
    int startX = xMin_ + localXOffset;
    int startY = yMin_ + localYOffset;
    display_->drawBox(startX + xStart, startY + yStart, startX + xEnd, startY + yEnd);
}
