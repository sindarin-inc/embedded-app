#include "LetsFocusWelcomeMenuScreen.hpp"

LetsFocusWelcomeMenuScreen::LetsFocusWelcomeMenuScreen(DisplaySystem &display,
                                                       PreferencesStore &prefs)
    : EinkView(display), prefs_(prefs), menuView_(display),
      renderer_(display, DisplayRect({0, 0, 0, 0}), static_cast<Font *>(&fontFace0)),
      rendererView_(display, renderer_),
      leftArrowView_(display, "Prev", ArrowDirection::ArrowLeft, ArrowSide::Left),
      rightArrowView_(display, "Next", ArrowDirection::ArrowRight, ArrowSide::Right) {
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
    case RemoteCommand::NONE:
    case RemoteCommand::ANIMATION:
        break;
    }
    setNeedsLayout();
    if (textSelection_ == texts_.size() - 1) {
        prefs_.seenLetsFocus.set(true);
    }
}

auto LetsFocusWelcomeMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    // We need to push the left arrow view all the way to the left of the screen
    // and the right arrow view all the way to the right of the screen. That's why the -15 and + 30
    // to size.width below.
    context.setViewFrame(leftArrowView_, {{-15, 1}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical);

    context.setViewFrame(rightArrowView_, {{-15, 1}, {size.width + 30, size.height}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical |
                             LayoutOptionsAlignRight);

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
    if (textSelection_ != 0) {
        leftArrowView_.draw();
    }
    if (textSelection_ != texts_.size() - 1) {
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