#include "UI/LegalScreen.hpp"

#include "sindarin-debug.h"

auto LegalScreen::layout(LayoutContext &context, Size size) -> Size {
    uint8_t leftPadding = 10;
    uint8_t yAdvance = 20;

    int x = 0;
    int y = 0;

    x += leftPadding;

    context.setViewFrame(productNameView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 8;

    context.setViewFrame(serialNumberView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    // y += yAdvance;

    // context.setViewFrame(firmwareVersionView_, {{x, y}, size},
    //                      LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 3;

    context.setViewFrame(remoteFirmwareVersionView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 3;

    context.setViewFrame(remoteBatteryView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    context.setViewFrame(returnToMenuView_, {{0, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                             LayoutOptionsAlignBottom);

    y += yAdvance - 1;

    context.setViewFrame(fccView_, {{10, y}, size}, LayoutOptionsAlignLeft);

    y += 5;

    context.setViewFrame(ceView_, {{110, y}, size}, LayoutOptionsAlignLeft);

    y += 65;

    context.setViewFrame(baclView_, {{0, y}, size}, LayoutOptionsAlignLeft);

    return size;
}

void LegalScreen::drawInRect(Rect rect) {
    productNameView_.draw();
    serialNumberView_.draw();
    // firmwareVersionView_.draw();
    remoteFirmwareVersionView_.draw();
    remoteBatteryView_.draw();
    fccView_.draw();
    ceView_.draw();
    baclView_.draw();
    returnToMenuView_.draw();
}

void LegalScreen::render(DisplaySystem &display) {
    if (!needsRerender_) {
        return;
    }
    setNeedsLayout();
    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});
    draw();
    display.updateDisplay();
    needsRerender_ = false;
}

void LegalScreen::processCommand(RemoteCommand command) {
    if (command == secretCodeCommands_[secretCodeIndex_]) {
        log_d("Got the right code %d time(s)!", secretCodeIndex_ + 1);
        secretCodeIndex_++;
        if (secretCodeIndex_ == secretCodeCommands_.size()) {
            log_d("Got the right secret code -- toggling Debug Screen and rebooting!");
            this->prefs_.showDebugMenu.set(!this->prefs_.showDebugMenu.get().value_or(false));
            power_.reboot();
        }
    } else {
        secretCodeIndex_ = 0;
    }
    if (command == RemoteCommand::ENTER) {
        back();
    }
}
