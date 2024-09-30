#include "LinkedMenuScreen.hpp"

extern RootUI rootUi;

LinkedMenuScreen::LinkedMenuScreen(DisplaySystem &display, std::string backLabel)
    : EinkView(display), menuView_(display), backLabelView_(display),
      backLabel_(std::move(backLabel)), rendererView_(display, 4) {
    menuView_.padding = 5;
    menuView_.itemsPerPage = 4;
    menuView_.indicatorWidth = 5;
    menuView_.indicatorPadding = 5;
    menuView_.pagingIndicatorWidth = 5;
    menuView_.pagingIndicatorPadding = 5;
    menuView_.setIndicatorType(NewMenuView::IndicatorType::Line);
    backLabelView_.setText(backLabel_);

    rendererView_.setText(
        "You open an ancient, dusty book, its pages crackling under your fingers. A strange energy "
        "fills the room, pulling you deeper into the story.");
    rendererView_.alignCenter = true;
}

void LinkedMenuScreen::addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void LinkedMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        moveSelection(-1);
        break;
    case RemoteCommand::DOWN:
        moveSelection(1);
        break;
    case RemoteCommand::ENTER:
        menuView_.executeSelectedItem();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        if (previousScreen_) {
            gotoPreviousScreen();
        }
        break;
    case RemoteCommand::RIGHT:
        if (nextScreen_) {
            gotoNextScreen();
        }
        break;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

auto LinkedMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int y = 0;

    // Make it 30px wide max
    context.setViewFrame(rendererView_, {{size.width / 2 - 90, size.height / 2 - 100}, {180, 180}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    context.setViewFrame(menuView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);

    return size;
}

void LinkedMenuScreen::drawInRect(Rect rect) {
    rendererView_.draw();
    menuView_.draw();
}

void LinkedMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}

void LinkedMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}