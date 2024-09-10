#include "SystemSettingsMenuScreen.hpp"

#include "Renderers/Renderer.hpp"
#include "UI/MsgPopup.hpp"

SystemSettingsMenuScreen::SystemSettingsMenuScreen(DisplaySystem &display,
                                                   const std::string &backTitle)
    : EinkView(display), menuView_(display), backLabelView_(display, backTitle) {
    menuView_.padding = 1;
    menuView_.itemsPerPage = 12;
}

void SystemSettingsMenuScreen::addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void SystemSettingsMenuScreen::processCommand(RemoteCommand command) {
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
        back();
        setNeedsRerender();
        break;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

auto SystemSettingsMenuScreen::setSelectedIndex(size_t index) -> bool {
    auto newPage = menuView_.setSelectedIndex(index);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
    return newPage;
}

auto SystemSettingsMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    // Layout back label
    context.setViewFrame(backLabelView_, {{0, 0}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);
    // Layout menu
    context.setViewFrame(menuView_, Rect::fromSize(size).inset(size.width / 7, 0),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical);

    return size;
}

void SystemSettingsMenuScreen::showTemporaryMessage(DisplaySystem &display,
                                                    const std::string &message, int duration,
                                                    bool rerenderAfter) {
    auto *popup = new MsgPopup(display, message);
    popup->render(display);
    delete popup;
    if (rerenderAfter) {
        Delay(duration);
        setNeedsRerender();
        this->render(display);
    }
}

void SystemSettingsMenuScreen::drawInRect(Rect rect) {
    backLabelView_.draw();
    menuView_.draw();
}

void SystemSettingsMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

void SystemSettingsMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawIndicator();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}
