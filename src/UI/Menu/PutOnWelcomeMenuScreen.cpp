#include "PutOnWelcomeMenuScreen.hpp"

PutOnWelcomeMenuScreen::PutOnWelcomeMenuScreen(DisplaySystem &display)
    : EinkView(display), menuView_(display),
      renderer_(display, DisplayRect({0, 0, 0, 0}), static_cast<Font *>(&fontFace0)),
      rendererView_(display, renderer_) {
    menuView_.padding = 5;
    menuView_.itemsPerPage = 4;
    menuView_.indicatorWidth = 5;
    menuView_.indicatorPadding = 5;
    menuView_.pagingIndicatorWidth = 5;
    menuView_.pagingIndicatorPadding = 5;
    menuView_.setIndicatorType(NewMenuView::IndicatorType::Line);

    std::string message =
        "Welcome to your Sol Reader!\n\nYou've taken the first steps towards a new way to enjoy "
        "comfortable, distraction-free reading. These welcome steps will help you get set up in "
        "no time at all.\n\nBut if setup isn't your thing, simply head back to the Main Menu and "
        "press   \ue002   to get to Books or   \ue001   to get to Settings.";

    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);
    renderer_.setLineSpacing(-1);
}

void PutOnWelcomeMenuScreen::addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void PutOnWelcomeMenuScreen::processCommand(RemoteCommand command) {
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

auto PutOnWelcomeMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(rendererView_, Rect::fromSize(size).inset(10, 10),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    context.setViewFrame(menuView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);

    return size;
}

void PutOnWelcomeMenuScreen::drawInRect(Rect rect) {
    rendererView_.draw();
    menuView_.draw();
}

void PutOnWelcomeMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}

void PutOnWelcomeMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}