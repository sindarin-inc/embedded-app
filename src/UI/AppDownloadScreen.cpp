#include "UI/AppDownloadScreen.hpp"

#include "Misc/Dispatch.hpp"
#include "Storage/PreferencesStore.hpp"
#include "sindarin-debug.h"

auto AppDownloadScreen::layout(LayoutContext &context, SolSize size) -> SolSize {

    // Center the QR code as the layout anchor
    auto iconViewRect = context.setViewFrame(iconView_, {{0, 0}, {size.width, size.height}},
                                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    // Now put the top text on top of the QR code
    context.setViewSizeInRect(topLineView_, {180, 100},
                              {{0, 0}, {size.width, iconViewRect.origin.y - 7}},
                              LayoutOptionsSizeToFitVertical | LayoutOptionsAlignBottom |
                                  LayoutOptionsAlignCenterHorizontal);

    // And put the bottom text below the QR code
    auto y = iconViewRect.bottom() + 5;
    context.setViewFrame(bottomLine_, {{0, y}, {size.width, size.height - y}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);

    return size;
}

void AppDownloadScreen::drawInRect(SolRect rect) {
    topLineView_.draw();
    iconView_.draw();
    bottomLine_.draw();
}

void AppDownloadScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect().inset(-5));
    draw();
    display.updateDisplay();
}

void AppDownloadScreen::processCommand(RemoteCommand command) { back(); }

extern void UIInnerLoop(RemoteCommand command);

void AppDownloadScreen::timerCallback(TimerHandle_t timer) {
    // If paired status goes to paired, go back
    if (PreferencesStore::paired == PairedStatus::Paired) {
        auto *instance = reinterpret_cast<AppDownloadScreen *>(pvTimerGetTimerID(timer));
        DispatchGetUiQueue()->dispatch([instance]() {
            if (instance->makeAndStartContentSync_) {
                instance->makeAndStartContentSync_();
            } else {
                UIInnerLoop(RemoteCommand::BACK);
            }
        });
        xTimerStop(timer, 0);
    }
}
