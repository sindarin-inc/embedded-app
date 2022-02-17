#include "CalibrationScreen.hpp"

#include "UI/Fonts/LucidaGrande5pt7b.h"

#define NUM_SPIKES 24

void CalibrationScreen::drawSiemensStar(DisplaySystem &display, u_int32_t originX,
                                        u_int32_t originY, u_int32_t radius) {
    // Lot's of precision ;)
    float pi = 3.141592653585753;
    float theta[NUM_SPIKES];
    float x[NUM_SPIKES];
    float y[NUM_SPIKES];

    int i, k;

    for (i = 0; i < NUM_SPIKES; i++) {

        theta[i] = -pi + i * (2 * pi / NUM_SPIKES);

        x[i] = radius * cosf(theta[i]) + originX;
        y[i] = radius * sinf(theta[i]) + originY;
    }

    for (k = 0; k < NUM_SPIKES / 2; k++) {
        display.fillTriangle(originX, originY, x[k * 2], y[k * 2], x[k * 2 + 1], y[k * 2 + 1],
                             display.fg);
    }
}

void CalibrationScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    if (!opticsTest) {
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
    } else {
        float oldX = display.getFocusWidth();
        float oldY = display.getFocusHeight();

        display.setFocusSize(23.4, 23.4);

        int i, l = 0;

        for (l = 0; l < 32; l++) {
            for (i = 0; i < (display.xMax) / 4; i++) {
                // only every other
                if (i % 2 == 0) {
                    display.fillRect(display.xMin + i * 4, display.yMin + l * 8, 4, 4, display.fg);
                }
            }

            for (i = 0; i < (display.xMax) / 4; i++) {
                // only every other
                if (i % 2 == 1) {
                    display.fillRect(display.xMin + i * 4, display.yMin + 4 + l * 8, 4, 4,
                                     display.fg);
                }
            }
        }

        display.fillRect(0, 0, 64, 64, display.bg);

        display.fillRect(128 - 32, 128 - 32, 64, 64, display.bg);

        display.fillRect(256 - 64, 256 - 64, 64, 64, display.bg);

        display.fillRect(0, 256 - 64, 64, 64, display.bg);

        display.fillRect(256 - 64, 0, 64, 64, display.bg);

        drawSiemensStar(display, 128, 128, 32);
        drawSiemensStar(display, 32, 32, 32);
        drawSiemensStar(display, 256 - 32, 32, 32);
        drawSiemensStar(display, 256 - 32, 256 - 32, 32);
        drawSiemensStar(display, 32, 256 - 32, 32);

        display.updateDisplay();
        display.setFocusSize(oldX, oldY);
    }

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