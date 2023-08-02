#include "UI/LegalScreen.hpp"

#include "sindarin-debug.h"

auto LegalScreen::layout(LayoutContext &context, Size size) -> Size {
    uint8_t leftPadding = 5;
    uint8_t yAdvance = 20;

    int x = 0;
    int y = 10;

    x += leftPadding;

    context.setViewFrame(baclLine1View_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 8;

    context.setViewFrame(baclLine2View_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 4;

    context.setViewFrame(fcc1LineView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 8;

    context.setViewFrame(fcc1LineViewPart2_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 4;

    context.setViewFrame(fcc2LineView_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 8;

    context.setViewFrame(fcc2LineViewPart2_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 4;

    context.setViewFrame(opTemp1_, {{x, y}, size}, LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 8;

    context.setViewFrame(opTemp2_, {{x, y}, size}, LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 4;

    context.setViewFrame(nonOpTemp1_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 8;

    context.setViewFrame(nonOpTemp2_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 4;

    context.setViewFrame(opAlt_, {{x, y}, size}, LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 4;

    context.setViewFrame(relativeHumidity1_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    y += yAdvance - 8;

    context.setViewFrame(relativeHumidity2_, {{x, y}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

    context.setViewFrame(leftRightView_, {{0, -yAdvance + 4}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                             LayoutOptionsAlignBottom);

    context.setViewFrame(returnToMenuView_, {{0, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                             LayoutOptionsAlignBottom);

    y = yAdvance * 2;

    context.setViewFrame(ceView_, {{10, y}, size}, LayoutOptionsAlignLeft);

    y -= 5;

    context.setViewFrame(fccView_, {{110, y}, size}, LayoutOptionsAlignLeft);

    y += 65;

    context.setViewFrame(baclView_, {{0, y}, size}, LayoutOptionsAlignLeft);

    return size;
}

void LegalScreen::drawInRect(Rect rect) {
    if (!logosPage_) {
        baclLine1View_.draw();
        baclLine2View_.draw();
        fcc1LineView_.draw();
        fcc1LineViewPart2_.draw();
        fcc2LineView_.draw();
        fcc2LineViewPart2_.draw();
        opTemp1_.draw();
        opTemp2_.draw();
        nonOpTemp1_.draw();
        nonOpTemp2_.draw();
        opAlt_.draw();
        relativeHumidity1_.draw();
        relativeHumidity2_.draw();
    } else {
        fccView_.draw();
        ceView_.draw();
        baclView_.draw();
    }
    leftRightView_.draw();
    returnToMenuView_.draw();
}

void LegalScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    int adjustedXPadding = display.defaultXPadding - 5;
    int adjustedYPadding = display.defaultYPadding - 10;
    setFrame({{display.xAbsolute(adjustedXPadding), display.yAbsolute(adjustedYPadding)},
              {display.defaultScreenWidth - 2 * adjustedXPadding,
               display.defaultScreenHeight - 2 * adjustedYPadding}});
    draw();
    display.updateDisplay();
}

void LegalScreen::processCommand(RemoteCommand command) {
    // Ignore all other presses to match About menu but no secret code here.
    if (command == RemoteCommand::ENTER) {
        back();
    }
    if (command == RemoteCommand::LEFT || command == RemoteCommand::RIGHT) {
        logosPage_ = !logosPage_;
        setNeedsRerender();
    }
}
