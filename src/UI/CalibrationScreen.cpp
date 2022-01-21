#include "CalibrationScreen.hpp"

#include "UI/Fonts/LucidaGrande5pt7b.h"

void CalibrationScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    // TODO: DisplaySystem should just handle this stuff
    int xMin = display.localXOffset + display.xMin;
    int yMin = display.localYOffset + display.yMin;
    int xMax = display.localXOffset + display.xMax;
    int yMax = display.localYOffset + display.yMax;

    const int edgeOffset = 5;
    int midpointX = ((xMax - xMin) / 2) + xMin;
    int midpointY = ((yMax - yMin) / 2) + yMin;
    int boxWidth = xMax - xMin;
    int boxHeight = yMax - yMin;

    display.fillRect(xMin, yMin, boxWidth, boxHeight, display.fg);
    display.fillRect(xMin + 2, yMin + 2, boxWidth - 4, boxHeight - 4, display.bg);
    display.setFont(&LUCIDA_GRANDE5PT7B);

    int letsWidth = display.getTextWidth("let's");
    int focusWidth = display.getTextWidth("focus");
    int letsHeight = display.getTextHeight();
    int focusHeight = display.getTextHeight();

    for (int i = 30; i > 23; i--) {
        display.drawCircle(midpointX, midpointY, i, display.fg);
    }

    display.setCursor(midpointX - letsWidth / 2, midpointY - 3);
    display.println("let's");
    display.setCursor(midpointX - focusWidth / 2, midpointY + focusHeight - 6);
    display.println("focus");

    display.setCursor(xMax - display.getTextWidth("lower right") - edgeOffset,
                      yMax - edgeOffset - 2);
    display.println("lower right");
    display.setCursor(xMin + edgeOffset, yMax - edgeOffset - 2); // lower left
    display.println("lower left");
    display.setCursor(xMin + edgeOffset,
                      yMin + display.getTextHeight() - 2); // upper left
    display.println("upper left");
    display.setCursor(xMax - display.getTextWidth("upper right") - edgeOffset,
                      yMin + display.getTextHeight() - 2);
    display.println("upper right");

    display.updateDisplay();

    needsRerender = false;
}

void CalibrationScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
    case RemoteCommand::CHANNEL_PLUS:
    case RemoteCommand::PREV:
    case RemoteCommand::NEXT:
    case RemoteCommand::BACK:
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    default:
        break;
    }
}