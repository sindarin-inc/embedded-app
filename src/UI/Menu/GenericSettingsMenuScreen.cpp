#include "GenericSettingsMenuScreen.hpp"

#include "Renderers/Renderer.hpp"

GenericSettingsMenuScreen::GenericSettingsMenuScreen(DisplaySystem &display,
                                                     const std::string &backTitle)
    : EinkView(display), menuView_(display), backLabelView_(display, backTitle) {
    menuView_.padding = 1;
    menuView_.itemsPerPage = 12;
}

void GenericSettingsMenuScreen::addMenuItem(const std::shared_ptr<MenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void GenericSettingsMenuScreen::processCommand(RemoteCommand command) {
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

auto GenericSettingsMenuScreen::setSelectedIndex(size_t index) -> bool {
    auto newPage = menuView_.setSelectedIndex(index);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
    return newPage;
}

auto GenericSettingsMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    // Layout back label
    context.setViewFrame(backLabelView_, {{0, 0}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);
    // Layout menu
    context.setViewFrame(menuView_, Rect::fromSize(size).inset(size.width / 7, 0),
                         LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    return size;
}

void GenericSettingsMenuScreen::drawInRect(Rect rect) {
    backLabelView_.draw();
    menuView_.draw();
}

void GenericSettingsMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

void GenericSettingsMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();

    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
