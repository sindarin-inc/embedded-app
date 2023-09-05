#include "UI/LegalScreen.hpp"

#include "sindarin-debug.h"

auto LegalScreen::layout(LayoutContext &context, Size size) -> Size {
    uint8_t leftPadding = 5;
    uint8_t yAdvance = 16;
    const uint8_t lineGroupPadding = 4;

    int x = 0;
    int y = 10;

    x += leftPadding;

    context.setViewFrame(baclLine1View_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance;

    context.setViewFrame(baclLine2View_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance - lineGroupPadding;

    context.setViewFrame(fcc1LineView_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance;

    context.setViewFrame(fcc1LineViewPart2_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance - lineGroupPadding;

    context.setViewFrame(fcc2LineView_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance;

    context.setViewFrame(fcc2LineViewPart2_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance - lineGroupPadding;

    context.setViewFrame(opTemp1_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance;

    context.setViewFrame(opTemp2_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance - lineGroupPadding;

    context.setViewFrame(nonOpTemp1_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance;

    context.setViewFrame(nonOpTemp2_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance - lineGroupPadding;

    context.setViewFrame(opAlt_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance - lineGroupPadding;

    context.setViewFrame(relativeHumidity1_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    y += yAdvance;

    context.setViewFrame(relativeHumidity2_, {{x, y}, size}, LayoutOptionsSizeToFitVertical);

    context.setSublayoutFrame(
        {{0, 0}, size}, LayoutOptionsSizeToFitVertical | LayoutOptionsAlignBottom,
        [this](LayoutContext &context, Size size) -> Size {
            int y = 0;
            auto leftRightRect =
                context.setViewFrame(leftRightView_, {{0, y}, {size.width, 0}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
            y += leftRightRect.size.height + 2;
            auto returnToMenuRect =
                context.setViewFrame(returnToMenuView_, {{0, y}, {size.width, 0}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
            y += returnToMenuRect.size.height;
            return {size.width, y};
        });

    context.setSublayoutFrame(
        {{0, -15}, size}, LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int ukcaPadding = 22;
            int y = 0;
            auto baclRect =
                context.setViewFrame(baclView_, {{0, y}, {size.width, 0}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
            y += baclRect.size.height + 5;
            auto fccRect =
                context.setViewFrame(fccView_, {{0, y}, {size.width, 0}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
            y += fccRect.size.height + 5;
            context.setViewFrame(ceView_, {{ukcaPadding, y}, {size.width - ukcaPadding * 2, 0}},
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);

            auto ukcaRect = context.setViewFrame(
                ukcaView_, {{ukcaPadding, y}, {size.width - ukcaPadding * 2, 0}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignRight);

            y += ukcaRect.size.height;

            return {size.width, y};
        });

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
        ukcaView_.draw();
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
