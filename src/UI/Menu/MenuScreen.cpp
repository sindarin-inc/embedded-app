#include "MenuScreen.hpp"

MenuScreen::MenuScreen(DisplaySystem &display, std::string title)
    : EinkView(display), menuView_(display), titleView_(display), title_(std::move(title)) {
    titleView_.setText(title_);
}

void MenuScreen::addMenuItem(const std::shared_ptr<MenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void MenuScreen::processCommand(RemoteCommand command) {
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
        firstLoad_ = true;
        setNeedsRerender();
        back();
        return;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

auto MenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int y = 0;

    // Layout title
    context.setViewFrame(titleView_, {{0, y}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);

    // Layout menu
    context.setViewFrame(menuView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    return size;
}

void MenuScreen::drawInRect(Rect rect) {
    titleView_.draw();
    menuView_.draw();
}

void MenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

// Implement Screen method
void MenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
