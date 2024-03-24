#include "UI/AppDownloadScreen.hpp"

#include "sindarin-debug.h"

auto AppDownloadScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setSublayoutFrame(
        Rect::fromSize(size), LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int y = 0;

            // Text with a box around it
            auto topLineViewRect =
                context.setViewFrame(topLineView_, {{0, y}, {size.width, 100}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
            y += topLineViewRect.size.height + 7;

            auto iconViewRect =
                context.setViewFrame(iconView_, {{0, y}, {size.width, 200}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
            y += iconViewRect.size.height + 5;

            auto bottomLineRect =
                context.setViewFrame(bottomLine_, {{0, y}, {size.width, 100}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
            y += bottomLineRect.size.height;
            return {size.width, y};
        });

    return size;
}

void AppDownloadScreen::drawInRect(Rect rect) {
    topLineView_.draw();
    iconView_.draw();
    bottomLine_.draw();
}

void AppDownloadScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    int adjustedXPadding = display.defaultXPadding - 5;
    int adjustedYPadding = display.defaultYPadding - 5;
    setFrame({{display.xAbsolute(adjustedXPadding), display.yAbsolute(adjustedYPadding)},
              {display.defaultScreenWidth - 2 * adjustedXPadding,
               display.defaultScreenHeight - 2 * adjustedYPadding}});
    draw();
    display.updateDisplay();
}

void AppDownloadScreen::processCommand(RemoteCommand command) { back(); }
