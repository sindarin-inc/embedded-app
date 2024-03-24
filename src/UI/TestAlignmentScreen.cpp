#include "TestAlignmentScreen.hpp"

#include <cmath>

#include "Displays/EinkTypes.hpp"
#include "Misc/Style.hpp"

void DrawEvenCircle(Adafruit_GFX *gfx, int16_t centerX, int16_t centerY, int16_t radius,
                    uint16_t color) {
    // Draw the left semi-circle
    for (int16_t y = -radius; y <= radius; y++) {
        auto x = static_cast<int16_t>(sqrt(radius * radius - y * y));
        gfx->drawPixel(centerX - x, centerY + y, color);
    }

    // Draw the right semi-circle (1 pixel over to create an even width)
    for (int16_t y = -radius; y <= radius; y++) {
        auto x = static_cast<int16_t>(sqrt(radius * radius - y * y));
        gfx->drawPixel(centerX + x - 1, centerY + y, color);
    }
}
void DrawCenteredCircle(Adafruit_GFX *gfx, int16_t centerX, int16_t centerY, uint16_t radius,
                        uint16_t color) {
    // Draw the upper semicircle
    for (int16_t y = -radius; y <= 0; y++) {
        int16_t x = sqrt(radius * radius - y * y);
        gfx->drawLine(centerX - x, centerY + y, centerX + x, centerY + y, color);
    }

    // Draw the lower semicircle
    for (int y = 0; y <= radius; y++) {
        int16_t x = sqrt(radius * radius - y * y);
        gfx->drawLine(centerX - x, centerY + y, centerX + x, centerY + y, color);
    }
}
void TestAlignmentScreen::render(DisplaySystem &display) {
    const int16_t padding = 33;
    constexpr int16_t numLines = 10;
    int16_t w = display.absoluteWidth();
    int16_t h = display.absoluteHeight();
    const int16_t lineSpacing = (w - 2 * padding - numLines) / (numLines - 1); // Line spacing
    w = w - 1;
    h = h - 1;
    const int16_t radius = 5;
    const int16_t outerCircleRadius = 9;

    auto color = display.fg;

    // Draw horizontal lines
    for (int i = 0; i < numLines; i++) {
        int16_t y = padding + i * (lineSpacing + 1);
        display.gfxClass()->drawLine(padding, y, w - padding, y, color);
    }

    // Draw vertical lines
    for (int i = 0; i < numLines; i++) {
        int16_t x = padding + i * (lineSpacing + 1);
        display.gfxClass()->drawLine(x, padding, x, h - padding, color);
    }

    // Draw the circles
    display.gfxClass()->fillCircle(padding, padding, radius, color);            // Top-left corner
    display.gfxClass()->drawCircle(padding, padding, outerCircleRadius, color); // Outer circle
    display.gfxClass()->fillCircle(w - padding, padding, radius, color);        // Top-right corner
    display.gfxClass()->drawCircle(w - padding, padding, outerCircleRadius,
                                   color); // Outer circle
    display.gfxClass()->fillCircle(padding, h - padding, radius,
                                   color); // Bottom-left corner
    display.gfxClass()->drawCircle(padding, h - padding, outerCircleRadius,
                                   color); // Outer circle
    display.gfxClass()->fillCircle(w - padding, h - padding, radius,
                                   color); // Bottom-rightish corner
    display.gfxClass()->drawCircle(w - padding, h - padding, outerCircleRadius,
                                   color); // Outer circle

    // Adafruit_GFX doesn't have a method to draw a filled circle with an even width, so their docs
    // recommend drawing a rounded rect instead
    // See: https://cdn-learn.adafruit.com/downloads/pdf/adafruit-gfx.pdf (search 'If an even-sized
    // circle is required')
    display.gfxClass()->drawRoundRect(w / 2 - outerCircleRadius + 1, h / 2 - outerCircleRadius + 1,
                                      2 * outerCircleRadius, 2 * outerCircleRadius,
                                      outerCircleRadius, color);

    display.gfxClass()->fillRoundRect(w / 2 - radius + 1, h / 2 - radius + 1, 2 * radius,
                                      2 * radius, radius, color);

    display.updateDisplay(DisplaySystem::UpdateType::UNSPECIFIED, false);

    // Draw a little extra line on the right screen
    int16_t y = padding + 4 * (lineSpacing + 1);
    display.gfxClass()->drawLine(w - padding, y, w - padding + 10, y, color);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false, 0,
                          DisplaySystem::UpdateType::UNSPECIFIED, ScreenSide::Right);

    // Erase the little line on the right screen
    display.gfxClass()->drawLine(w - padding + 1, y, w - padding + 10, y, display.bg);
    // Draw a little line on the left screen
    display.gfxClass()->drawLine(padding - 10, y, padding, y, color);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true, 0,
                          DisplaySystem::UpdateType::UNSPECIFIED, ScreenSide::Left);

    log_i("Grid rendered");
}

void TestAlignmentScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT:
    case RemoteCommand::RIGHT:
    case RemoteCommand::UP:
    case RemoteCommand::DOWN:
    case RemoteCommand::BACK:
        back();
        break;
    default:
        break;
    }
}
