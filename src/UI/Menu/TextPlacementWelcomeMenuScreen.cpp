#include "TextPlacementWelcomeMenuScreen.hpp"

TextPlacementWelcomeMenuScreen::TextPlacementWelcomeMenuScreen(DisplaySystem &display)
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
    menuView_.setIndicatorType(MenuView::IndicatorType::Line);

    setText();
}

void TextPlacementWelcomeMenuScreen::setText() {
    std::string message;
    message += std::get<0>(texts_[textSelection_]);
    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);
    setNeedsLayout();
}

void TextPlacementWelcomeMenuScreen::addMenuItem(const std::shared_ptr<MenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void TextPlacementWelcomeMenuScreen::processCommand(RemoteCommand command) {
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

auto TextPlacementWelcomeMenuScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    auto arrowHeight = 9;

    context.setViewSizeInRect(leftArrowView_, {0, arrowHeight},
                              {{-15, 1}, {size.width, size.height}},
                              LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical);
    context.setViewSizeInRect(
        rightArrowView_, {0, arrowHeight}, {{-15, 1}, {size.width + 30, size.height}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical | LayoutOptionsAlignRight);

    context.setViewFrame(rendererView_,
                         SolRect::fromSize(size).inset(std::get<1>(texts_[textSelection_]).left,
                                                       std::get<1>(texts_[textSelection_]).right,
                                                       std::get<1>(texts_[textSelection_]).top,
                                                       std::get<1>(texts_[textSelection_]).bottom),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    if (textSelection_ == 0 || textSelection_ == 1) {
        context.setViewFrame(menuView_, SolRect::fromSize(size),
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                                 LayoutOptionsAlignBottom);
    } else {
        // We're going to put the menu at the top of the scren for the Bottom template
        context.setViewFrame(menuView_, SolRect::fromSize(size).inset(0, 15),
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal |
                                 LayoutOptionsAlignTop);
    }

    return size;
}

void TextPlacementWelcomeMenuScreen::drawInRect(SolRect rect) {
    switch (textSelection_) {
    case 0:
        renderer_.setLineSpacing(0);
        renderer_.setFont(&fontFace0);
        break;
    case 1:
        renderer_.setLineSpacing(-2);
        renderer_.setFont(&fontFace0);
        break;
    case 2:
        renderer_.setLineSpacing(0);
        renderer_.setFont(&fontFace1);
        break;
    }
    if (textSelection_ != 0) {
        leftArrowView_.draw();
    }
    if (textSelection_ != texts_.size() - 1) {
        rightArrowView_.draw();
    }
    rendererView_.draw();
    menuView_.draw();
}

void TextPlacementWelcomeMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}

void TextPlacementWelcomeMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}