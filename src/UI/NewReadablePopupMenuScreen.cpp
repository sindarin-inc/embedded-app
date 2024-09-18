#include "UI/NewReadablePopupMenuScreen.hpp"

#include "UI/MsgPopup.hpp"

void NewReadablePopupMenuScreen::setUpPopupMenu() {
    footer1TextView_.setText("15 Remaining");
    footer2TextView_.setText("312 of 790");
    footer3TextView_.setText("3:33pm");

    setNeedsLayout();
}

void NewReadablePopupMenuScreen::drawInRect(Rect rect) {

    popupMenuBackground_.fillColor = display_.bg;
    popupMenuBackground_.strokeColor = display_.bg;
    popupMenuForeground_.strokeColor = display_.fg;

    popupMenuBackground_.draw();
    popupMenuForeground_.draw();

    footer1TextView_.draw();
    footer2TextView_.draw();
    footer3TextView_.draw();

    titleView_.draw();
    menuView_.draw();
}

auto NewReadablePopupMenuScreen::layout(LayoutContext &context, Size size) -> Size {

    auto fullFrame = Rect::fromSize(size);

    context.setViewFrame(popupMenuBackground_, fullFrame);
    // Border is inset by 5 pixels on the top and bottom and then 1/4 of the width on the left and
    // right. This will need to be adjusted for different screen resolutions.
    auto foregroundFrame =
        context.setViewFrame(popupMenuForeground_, fullFrame.inset(size.width / 4, 5));
    auto contentFrame = foregroundFrame.inset(size.width / 4 + 5, 10);
    auto y = contentFrame.origin.y;
    auto x = 0;

    auto titleBox = context.setViewFrame(
        titleView_, {{contentFrame.origin.x, y}, contentFrame.size},
        LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignCenterHorizontal);

    y += titleBox.size.height + 10;

    // TODO: Fix me -- not sure if the MenuView needs to be changed to not do the ConstrainWidth in
    // order to handle the case where it's used in both the MainMenu and the PopupMenu
    auto menuBox = context.setViewFrame(
        menuView_, {{contentFrame.origin.x / 2 + 10, y}, contentFrame.size},
        LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignCenterHorizontal);

    y += menuBox.size.height + 2;

    // Nudge down the footers
    if (!footer1TextView_.getText().empty() || !footer2TextView_.getText().empty()) {
        y += 5;
    }

    auto footerRect = Rect({{contentFrame.origin.x, y}, {contentFrame.size.width, 0}});
    if (!footer1TextView_.getText().empty()) {
        auto footerTopRect =
            context.setViewFrame(footer1TextView_, footerRect,
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
        y += footerTopRect.size.height;
        footerRect.origin.y = y;
    }
    if (!footer2TextView_.getText().empty()) {
        auto footerBottomRect =
            context.setViewFrame(footer2TextView_, footerRect,
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
        y += footerBottomRect.size.height;
        footerRect.origin.y = y;
    }
    if (!footer3TextView_.getText().empty()) {
        auto footerBottomBottomRect =
            context.setViewFrame(footer3TextView_, footerRect,
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
        y += footerBottomBottomRect.size.height;
    }

    // Add bottom padding
    y += 5;

    // To be visually pleasing, the popup menu should take about 150% of the width
    // of the widest text -- this allows for the buttons as well
    // return {maxWidthOfText * 110 / 100, y};
    // 152 / 100 makes it exactly what it was before (106).
    // However, on other displays with larger resoltuions, the popup menu still seems a bit too
    // big. Future work to figure this out.
    return {152 * x / 100, y};
}

void NewReadablePopupMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    layoutIfNeeded();

    draw();

    if (firstLoad_) {
#if CONFIG_DISPLAY_EINKET013TT1
        // NOTE: This Screen is quirky since it is rendered directly from ReadableScreen. The
        // first time it is rendered, it will be rendered twice from ReadableScreen, once with
        // the text in the background (using PARTIAL) and once without the text in the
        // background (but using AU4). This creates the effect of the popup happening on top of
        // the text.
        firstLoad_ = false;
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, false);
#endif
    }
    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
};
