#include "InactivitySettingMenuScreen.hpp"

#include "Renderers/Renderer.hpp"

InactivitySettingMenuScreen::InactivitySettingMenuScreen(DisplaySystem &display,
                                                         const std::string &backTitle)
    : EinkView(display), menuView_(display), backLabelView_(display, backTitle),
      extraTextView_(display, "") {
    menuView_.padding = 1;
    menuView_.itemsPerPage = 12;
}

void InactivitySettingMenuScreen::addMenuItem(const std::shared_ptr<MenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void InactivitySettingMenuScreen::processCommand(RemoteCommand command) {
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

auto InactivitySettingMenuScreen::setSelectedIndex(size_t index) -> bool {
    auto newPage = menuView_.setSelectedIndex(index);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
    return newPage;
}

auto InactivitySettingMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    // Layout back label
    context.setViewFrame(backLabelView_, {{0, 0}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);
    // Layout menu
    context.setViewFrame(menuView_, Rect::fromSize(size).inset(size.width / 7, 0),
                         LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    context.setViewFrame(extraTextView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                             LayoutOptionsAlignCenterHorizontal);

    return size;
}

void InactivitySettingMenuScreen::drawInRect(Rect rect) {
    backLabelView_.draw();
    menuView_.draw();
    extraTextView_.draw();
}

void InactivitySettingMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

void InactivitySettingMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
