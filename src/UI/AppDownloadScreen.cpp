#include "UI/AppDownloadScreen.hpp"

#include "Misc/Dispatch.hpp"
#include "Storage/PreferencesStore.hpp"
#include "sindarin-debug.h"

auto AppDownloadScreen::layout(LayoutContext &context, Size size) -> Size {

    // Center the QR code as the layout anchor
    auto iconViewRect = context.setViewFrame(iconView_, {{0, 0}, {size.width, size.height}},
                                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    // Now put the top text on top of the QR code
    context.setViewSizeInRect(topLineView_, {100, 100},
                              {{0, 0}, {size.width, iconViewRect.origin.y - 7}},
                              LayoutOptionsSizeToFitVertical | LayoutOptionsAlignBottom |
                                  LayoutOptionsAlignCenterHorizontal);

    // And put the bottom text below the QR code
    auto y = iconViewRect.bottom() + 5;
    context.setViewFrame(bottomLine_, {{0, y}, {size.width, size.height - y}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);

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

extern void UIInnerLoop(RemoteCommand command);

void AppDownloadScreen::timerCallback(TimerHandle_t timer) {
    // If paired status goes to paired, go back
    if (PreferencesStore::paired == PairedStatus::Paired) {
        DispatchGetUiQueue()->dispatch([]() { UIInnerLoop(RemoteCommand::BACK); });
        xTimerStop(timer, 0);
    }
}
