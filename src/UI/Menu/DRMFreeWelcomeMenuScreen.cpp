#include "DRMFreeWelcomeMenuScreen.hpp"

DRMFreeWelcomeMenuScreen::DRMFreeWelcomeMenuScreen(DisplaySystem &display)
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

    std::string message;
    message +=
        "If you have a DRM-free EPUB, you can transfer it directly via USB using our desktop app."
        " Download the app from solreader.com/app. Direct USB transfers miss "
        "out on our Sol EPUB features like reading-in-app, automated AI summaries, dictionary, and "
        "more.\n\nTo access these features, get our mobile app, then pair your Sol Reader to the "
        "app. Finally, drop your DRM-Free EPUB to add.solreader.com and sync!";

    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);
    renderer_.setLineSpacing(-1);
}

void DRMFreeWelcomeMenuScreen::addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void DRMFreeWelcomeMenuScreen::processCommand(RemoteCommand command) {
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

auto DRMFreeWelcomeMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(rendererView_, Rect::fromSize(size).inset(10, 10),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    context.setViewFrame(menuView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);

    return size;
}

void DRMFreeWelcomeMenuScreen::drawInRect(Rect rect) {
    rendererView_.draw();
    menuView_.draw();
}

void DRMFreeWelcomeMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}

void DRMFreeWelcomeMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}