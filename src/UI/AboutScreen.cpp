#include "UI/AboutScreen.hpp"

#include "sindarin-debug.h"

auto AboutScreen::layout(LayoutContext &context, Size size) -> Size {
    uint8_t leftPadding = 20;
    uint8_t yAdvance = 20;

    int x = 0;
    int y = 10;

    context.setViewFrame(solLogoView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop |
                             LayoutOptionsAlignCenterHorizontal);

    y += sol_logo_height + 22;
    x += leftPadding;

    context.setViewFrame(productNameView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance;

    context.setViewFrame(serialNumberView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance;

    context.setViewFrame(firmwareVersionView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance;

    context.setViewFrame(remoteFirmwareVersionView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance;

    context.setViewFrame(remoteBatteryView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    context.setViewFrame(returnToMenuView_, {{0, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);
    return size;
}

void AboutScreen::drawInRect(Rect rect) {
    solLogoView_.draw();
    productNameView_.draw();
    serialNumberView_.draw();
    firmwareVersionView_.draw();
    remoteFirmwareVersionView_.draw();
    remoteBatteryView_.draw();
    returnToMenuView_.draw();
}

void AboutScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    int adjustedXPadding = display.defaultXPadding - 5;
    int adjustedYPadding = display.defaultYPadding - 5;
    setFrame({{display.xAbsolute(adjustedXPadding), display.yAbsolute(adjustedYPadding)},
              {display.defaultScreenWidth - 2 * adjustedXPadding,
               display.defaultScreenHeight - 2 * adjustedYPadding}});

    draw();
    display.updateDisplay();
}

void AboutScreen::processCommand(RemoteCommand command) {
    secretCode_.processCommand(command);
    if (command == RemoteCommand::ENTER) {
        back();
    }
}
