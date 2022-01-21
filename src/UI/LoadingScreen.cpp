#include "UI/LoadingScreen.hpp"

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

#define INTERVAL 1000
#define BOX_WIDTH 10
#define SPACING 5

void LoadingScreen::render() {
    auto now = millis();
    if (needsRerender || (now - timer_ > INTERVAL)) {
        // TODO: Would be nice to selectively clear parts of the screen
        if (state_ == 0) {
            display_.clearScreen();
        }
        int width = display_.width();
        int height = display_.height();
        int startX = (width / 2) - (3 * BOX_WIDTH + 2 * SPACING) / 2;
        int xOffset = state_ * (BOX_WIDTH + SPACING);
        int startY = (height / 2) - BOX_WIDTH / 2;
        // renderer_->renderBox(startX + xOffset, startY, startX + xOffset + BOX_WIDTH,
        //                      startY + BOX_WIDTH);
        // int startX = xMin_ + localXOffset;
        // int startY = yMin_ + localYOffset;
        // display_->drawBox(startX + xStart, startY + yStart, startX + xEnd, startY + yEnd);

        state_++;
        state_ = state_ % 3;
        timer_ = now;
        needsRerender = false;
    }
}

void LoadingScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::BACK:
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    default:
        break;
    }
}
