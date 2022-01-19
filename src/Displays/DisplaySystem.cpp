#include "DisplaySystem.hpp"

#include <utility>

#include "sindarin-debug.h"

DisplaySystem::DisplaySystem() {
    xToDraw_ = 0;
    yToDraw_ = 0;
    stringToDraw_ = "";
}

/*
void DisplaySystem::draw_box(int start_x, int start_y, int end_x, int end_y) {
    DEBUG_LN("Drawing box in wrong class :(");

}*/

void DisplaySystem::setX(int newX) { x_ = newX; }

void DisplaySystem::setY(int newY) { y_ = newY; }

void DisplaySystem::setFocusSize(float width, float height) {
    focusWidthInMm_ = width;
    focusHeightInMm_ = height;
}

auto DisplaySystem::getX() -> int { return x_; }

auto DisplaySystem::getY() -> int { return y_; }

void DisplaySystem::setWidth(float newWidth) { widthInMm_ = newWidth; }

void DisplaySystem::setHeight(float newHeight) { heightInMm_ = newHeight; }

auto DisplaySystem::getWidth() -> float { return widthInMm_; }

auto DisplaySystem::getHeight() -> float { return heightInMm_; }

void DisplaySystem::setPartialRefresh() { partialRefresh_ = 1; }

void DisplaySystem::unsetPartialRefresh() { partialRefresh_ = 0; }

auto DisplaySystem::getPartialRefresh() -> int { return partialRefresh_; }

void DisplaySystem::setCoordinates(int initX, int initY) {
    xToDraw_ = initX;
    yToDraw_ = initY;
}

void DisplaySystem::setText(const String &newText) { stringToDraw_ = newText; }

// Takes the member variable string_to_draw and splits it
// based on the \n characater and populates the String* lines;
void DisplaySystem::splitStringToLines() {
    int len, i, startPos, endPos;

    len = stringToDraw_.length();

    // Track where we are in the string
    startPos = 0;
    endPos = 0;
    numLines_ = 1;
    // if (Serial) Serial.println("Got to split");
    // if (Serial) Serial.println(string_to_draw);

    for (i = 0; i < len; i++) {
        // We found a new line. Split off the previous line and
        // start processing the new line
        // if (Serial) Serial.print(string_to_draw.charAt(i));
        if (stringToDraw_.charAt(i) == '\n') {
            // if (Serial) Serial.println("saw a new line");
            lines_[numLines_ - 1] = stringToDraw_.substring(startPos, endPos + 1);
            numLines_++;
            assert(numLines_ < MAX_LINES_IN_SPLIT);
            startPos = i + 1;
            endPos = i + 1;
        } else {
            endPos = i;
        }
    }

    // If \n is *not* the last character, we need to add the last line to the
    // array, but if \n is the last character, we don't need to do anything and
    // we can decrement num_lines since the above loop assumes that there will
    // be more characters after the \n.
    //
    // The second part of this || is to handle when there's just a single line with
    // a single character on it with no \n.
    if ((startPos != endPos) || stringToDraw_.charAt(endPos) != '\n')
        lines_[numLines_ - 1] = stringToDraw_.substring(startPos, endPos + 1);
    else
        numLines_--;

    // if (Serial) Serial.println("Finishing split");
}