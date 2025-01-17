#include "UI/AboutScreen.hpp"

#include "sindarin-debug.h"

auto AboutScreen::layout(SolUI::LayoutContext &context, SolUI::Size size) -> SolUI::Size {
    uint8_t leftPadding = 20;
    uint8_t yAdvance = 17;

    int x = 0;
    int y = 10;

    context.setViewFrame(solLogoView_, {{x, y}, size},
                         SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignTop |
                             SolUI::LayoutOptionsAlignCenterHorizontal);

    y += sol_logo_height + 22;
    x += leftPadding;

    context.setViewFrame(productNameView_, {{x, y}, size},
                         SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignLeft);

    y += yAdvance;

    context.setViewFrame(serialNumberView_, {{x, y}, size},
                         SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignLeft);

    y += yAdvance;

    context.setViewFrame(firmwareVersionView_, {{x, y}, size},
                         SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignLeft);

    y += yAdvance;

    context.setViewFrame(wifiMacView_, {{x, y}, size},
                         SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignLeft);

    y += yAdvance;

    context.setViewFrame(remoteFirmwareVersionView_, {{x, y}, size},
                         SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignLeft);

    y += yAdvance;

    context.setViewFrame(remoteBatteryView_, {{x, y}, size},
                         SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignLeft);

    context.setViewFrame(returnToMenuView_, {{0, 0}, size},
                         SolUI::LayoutOptionsSizeToFit | SolUI::LayoutOptionsAlignCenterHorizontal |
                             SolUI::LayoutOptionsAlignBottom);
    return size;
}

void AboutScreen::drawInRect(SolUI::Rect rect) {
    solLogoView_.draw();
    productNameView_.draw();
    serialNumberView_.draw();
    firmwareVersionView_.draw();
    wifiMacView_.draw();
    remoteFirmwareVersionView_.draw();
    remoteBatteryView_.draw();
    returnToMenuView_.draw();
}

void AboutScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect().inset(-5));
    draw();
    display.updateDisplay();
}

void AboutScreen::processCommand(RemoteCommand command) {
    secretCode_.processCommand(command);
    if (command == RemoteCommand::ENTER) {
        back();
    }
}
