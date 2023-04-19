#include "CalibrationScreen.hpp"

#include <cmath>

#include "Misc/Style.hpp"

#define NUM_SPIKES 24

void CalibrationScreen::drawSiemensStar(DisplaySystem &display, uint32_t originX, uint32_t originY,
                                        uint32_t radius) {
    display.gfxClass()->fillRect(originX - radius, originY - radius, radius * 2, radius * 2,
                                 display.bg);

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
        display.gfxClass()->fillTriangle(originX, originY, x[k * 2], y[k * 2], x[k * 2 + 1],
                                         y[k * 2 + 1], display.fg);
    }
}

void CalibrationScreen::render(DisplaySystem &display) {
    if (!opticsTest) {
        const int edgeOffset = 5;
        int midpointX = display.width() / 2;
        int midpointY = display.height() / 2;

        display.fillRect(0, 0, display.width(), display.height(), display.fg);
        display.fillRect(2, 2, display.width() - 4, display.height() - 4, display.bg);
        display.setFont(display.style.getFontFamily().readableFont);

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

        display.setCursor(display.width() - display.getTextWidth("lower right") - edgeOffset,
                          display.height() - edgeOffset - 2);
        display.println("lower right");
        display.setCursor(edgeOffset, display.height() - edgeOffset - 2); // lower left
        display.println("lower left");
        display.setCursor(edgeOffset,
                          display.getTextHeight() - 2); // upper left
        display.println("upper left");
        display.setCursor(display.width() - display.getTextWidth("upper right") - edgeOffset,
                          display.getTextHeight() - 2);
        display.println("upper right");

        display.updateDisplay();
    } else {
        int i, l = 0;

        for (l = 0; l < 32; l++) {
            for (i = 0; i < display.gfxClass()->width() / 4; i++) {
                // only every other
                if (i % 2 == 0) {
                    display.gfxClass()->fillRect(i * 4, l * 8, 4, 4, display.fg);
                }
            }

            for (i = 0; i < display.gfxClass()->width() / 4; i++) {
                // only every other
                if (i % 2 == 1) {
                    display.gfxClass()->fillRect(i * 4, 4 + l * 8, 4, 4, display.fg);
                }
            }
        }

        drawSiemensStar(display, 128, 128, 32);
        drawSiemensStar(display, 32, 32, 32);
        drawSiemensStar(display, 256 - 32, 32, 32);
        drawSiemensStar(display, 256 - 32, 256 - 32, 32);
        drawSiemensStar(display, 32, 256 - 32, 32);

        display.updateDisplay();
    }
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