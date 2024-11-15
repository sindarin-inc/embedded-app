#include "FirstBookWelcomeMenuScreen.hpp"

FirstBookWelcomeMenuScreen::FirstBookWelcomeMenuScreen(DisplaySystem &display)
    : EinkView(display), menuView_(display),
      renderer_(display, DisplayRect({0, 0, 0, 0}), static_cast<Font *>(&fontFace0)),
      rendererView_(display, renderer_) {
    menuView_.padding = 5;
    menuView_.itemsPerPage = 4;
    menuView_.indicatorWidth = 5;
    menuView_.indicatorPadding = 5;
    menuView_.pagingIndicatorWidth = 5;
    menuView_.pagingIndicatorPadding = 5;
    menuView_.setIndicatorType(MenuView::IndicatorType::Line);

    std::string message =
        "Let's get the first book on your Sol Reader!\n\nChoose your own adventure below.";

    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);
}

void FirstBookWelcomeMenuScreen::addMenuItem(const std::shared_ptr<MenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void FirstBookWelcomeMenuScreen::processCommand(RemoteCommand command) {
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
    case RemoteCommand::RIGHT:
    case RemoteCommand::NONE:
    case RemoteCommand::ANIMATION:
        break;
    }
    setNeedsLayout();
}

auto FirstBookWelcomeMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(rendererView_, Rect::fromSize(size).inset(45, 35, 65, 65),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    context.setViewFrame(menuView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);

    return size;
}

void FirstBookWelcomeMenuScreen::drawInRect(Rect rect) {
    rendererView_.draw();
    menuView_.draw();
}

void FirstBookWelcomeMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}

void FirstBookWelcomeMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}