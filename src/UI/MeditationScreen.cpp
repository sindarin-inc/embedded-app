#include "MeditationScreen.hpp"

#include "UI/Fonts/LucidaGrande5pt7b.h"
#include "UI/Fonts/LucidaGrande10pt7b.h"

void MeditationScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    display.setMeditativeMode(true);

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

    //display.fillRect(xMin, yMin, boxWidth, boxHeight, display.fg);
    //display.fillRect(xMin + 2, yMin + 2, boxWidth - 4, boxHeight - 4, display.bg);
    display.setFont(&LUCIDA_GRANDE10PT7B);

    int letsWidth = display.getTextWidth(messagesArray[placeInArray].c_str());
    int letsHeight = display.getTextHeight();

    /* Only do the circle the first time */

    if (placeInArray == 0) {
        display.fillCircle(midpointX, midpointY, 100, display.fg);
        display.fillCircle(midpointX, midpointY, 80, display.bg);
    }


    display.setCursor(midpointX - letsWidth / 2, midpointY + letsHeight / 2);
    display.println(messagesArray[placeInArray].c_str());
    /*

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

    */

    display.updateDisplay();

    if (placeInArray == 0) {
        delay(500); 
    } else {
        delay(1000);
    }

    if (placeInArray == numItemsInArray - 2) {
        delay(2000);
    }

    placeInArray++;
    if (placeInArray < numItemsInArray) {
        needsRerender = true; 
    }
    else {
        placeInArray = 0;
        display.setMeditativeMode(false);
        if (backCallback != nullptr) {
            backCallback();
        }
    }
}

void MeditationScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
    case RemoteCommand::BACK:
            if (backCallback != nullptr) {
            backCallback();
        }
    case RemoteCommand::CHANNEL_MINUS:
    case RemoteCommand::CHANNEL_PLUS:
    case RemoteCommand::NEXT:
        /* Intentional NOP for now */
    default:
        break;
    }
}