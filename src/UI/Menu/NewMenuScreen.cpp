#include "NewMenuScreen.hpp"

NewMenuScreen::NewMenuScreen(DisplaySystem &display, std::string title)
    : EinkView(display), menuView_(display), titleView_(display), title_(std::move(title)) {
    titleView_.setText(title_);
}

void NewMenuScreen::addMenuItem(std::shared_ptr<NewMenuItem> menuItem) {
    menuView_.addMenuItem(std::move(menuItem));
}

void NewMenuScreen::processCommand(RemoteCommand command) {
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
        // Handle back action (e.g., close this screen)
        break;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

auto NewMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int y = 0;

    // Layout title
    auto titleRect =
        context.setViewFrame(titleView_, {{0, y}, {size.width, 0}},
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
    y += titleRect.size.height + 10; // Add some spacing after the title

    // Layout menu
    context.setViewFrame(menuView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    return size;
}

void NewMenuScreen::drawInRect(Rect rect) {
    titleView_.draw();
    menuView_.draw();
}

void NewMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

// Implement Screen method
void NewMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawIndicator();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
