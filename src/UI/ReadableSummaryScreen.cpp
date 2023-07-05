#include "ReadableSummaryScreen.hpp"

#include "Misc/SpiramAllocator.hpp"

auto ReadableSummaryScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(leftArrowView_, {{0, -2}, {10, 5}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical |
                             LayoutOptionsAlignLeft);
    context.setViewFrame(upperLeftTextView_, {{15, -5}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);
    context.setViewFrame(rightArrowView_, {{size.width, size.height + 3}, {10, 5}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                             LayoutOptionsAlignLeft);
    context.setViewFrame(bottomRightTextView_, {{0, size.height}, {size.width - 4, 10}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical |
                             LayoutOptionsAlignRight);

    return {size.width, size.height - 10};
};

void ReadableSummaryScreen::drawInRect(Rect rect) {
    leftArrowView_.draw();
    upperLeftTextView_.draw();
    rightArrowView_.draw();
    bottomRightTextView_.draw();
}

void ReadableSummaryScreen::setPlaceInReadable(unsigned long placeInReadable) {
    placeInReadable_ = placeInReadable;
}

auto ReadableSummaryScreen::checkAtEnd() -> bool { return (newPlace_ == 0); }

void ReadableSummaryScreen::render(DisplaySystem &display) {

    setNeedsLayout();

    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    if (showUnread_) {
        bottomRightText_ = "Hide Unread Chapters";
    } else {
        bottomRightText_ = "Show Unread Chapters";
    }

    bottomRightTextView_.setText(bottomRightText_);
    rightArrowView_.setNeedsLayout();

    PROFILE_START(ReadableSummaryScreenLayoutAndDraw);
    draw();
    PROFILE_END_LOG_US(ReadableSummaryScreenLayoutAndDraw);

    if (firstLoad_) {
        unsigned long placeInSummary = 0;
        SpiramString summaryString =
            readable_->summary(placeInReadable_, placeInSummary, showUnread_);

        auto stream = std::make_shared<StdStringStream>(summaryString);
        renderer_.setStream(stream);
        newPlace_ = renderer_.renderPage(placeInSummary);
        log_d("Place in readable: %lu, place in summary: %lu", placeInReadable_, placeInSummary);
    } else {
        prevPlace_ = newPlace_;
        newPlace_ = renderer_.renderPage(newPlace_);
    }

    atEnd_ = checkAtEnd();
    display.updateDisplay(DisplaySystem::UpdateType::FULL);
    firstLoad_ = false;
}

void ReadableSummaryScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        back();
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        showUnread_ = !showUnread_;
        setFirstLoad();
        setNeedsRerender();
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        if (atEnd_) {
            back();
        }

        setNeedsRerender();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        newPlace_ = renderer_.getPreviousPlace();
        setNeedsRerender();
        break;
    default:
        break;
    }
}
