#include "LetsFocusWelcomeMenuScreen.hpp"

LetsFocusWelcomeMenuScreen::LetsFocusWelcomeMenuScreen(DisplaySystem &display)
    : EinkView(display), menuView_(display),
      renderer_(display, DisplayRect({0, 0, 0, 0}), static_cast<Font *>(&fontFace0)),
      rendererView_(display, renderer_), leftArrowView_(display, ArrowDirection::ArrowLeft),
      rightArrowView_(display, ArrowDirection::ArrowRight) {
    menuView_.padding = 5;
    menuView_.itemsPerPage = 4;
    menuView_.indicatorWidth = 5;
    menuView_.indicatorPadding = 5;
    menuView_.pagingIndicatorWidth = 5;
    menuView_.pagingIndicatorPadding = 5;
    menuView_.setIndicatorType(NewMenuView::IndicatorType::Line);

    setText();
}

void LetsFocusWelcomeMenuScreen::setText() {
    auto stream = std::make_shared<StdStringStream>(std::get<0>(texts_[textSelection_]));
    renderer_.setStream(stream);
    setNeedsLayout();
}

void LetsFocusWelcomeMenuScreen::addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void LetsFocusWelcomeMenuScreen::processCommand(RemoteCommand command) {
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
        if (textSelection_ == 0) {
            return;
        }
        textSelection_ = (textSelection_ + texts_.size() - 1) % texts_.size();
        setText();
        setFirstLoad();
        setNeedsRerender();
        break;
    case RemoteCommand::RIGHT:
        if (textSelection_ == texts_.size() - 1) {
            return;
        }
        textSelection_ = (textSelection_ + 1) % texts_.size();
        setText();
        setFirstLoad();
        setNeedsRerender();
        break;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

auto LetsFocusWelcomeMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int y = 0;

    auto arrowHeight = 9;

    context.setViewSizeInRect(leftArrowView_, {0, arrowHeight}, {{0, 1}, {size.width, size.height}},
                              LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical);
    context.setViewSizeInRect(
        rightArrowView_, {0, arrowHeight}, {{0, 1}, {size.width, size.height}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical | LayoutOptionsAlignRight);

    context.setViewFrame(rendererView_,
                         Rect::fromSize(size).inset(std::get<1>(texts_[textSelection_]).left,
                                                    std::get<1>(texts_[textSelection_]).right,
                                                    std::get<1>(texts_[textSelection_]).top,
                                                    std::get<1>(texts_[textSelection_]).bottom),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    context.setViewFrame(menuView_, Rect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                             LayoutOptionsAlignBottom);

    return size;
}

void LetsFocusWelcomeMenuScreen::drawInRect(Rect rect) {
    if (textSelection_ == 1) {
        leftArrowView_.draw();
    }
    if (textSelection_ == 0) {
        rightArrowView_.draw();
    }
    rendererView_.draw();
    menuView_.draw();
}

void LetsFocusWelcomeMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}

void LetsFocusWelcomeMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}