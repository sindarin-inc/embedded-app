#include "ReadableSummaryScreen.hpp"

#include "Misc/SpiramAllocator.hpp"

auto ReadableSummaryScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(leftArrowView_, {{0, -5}, {10, 5}});
    context.setViewFrame(upperLeftTextView_, {{15, -8}, size});
    context.setViewSizeInRect(rightArrowView_, {10, 5}, {{0, 6}, size},
                              LayoutOptionsAlignBottom | LayoutOptionsAlignRight);
    context.setViewFrame(bottomRightTextView_, {{0, size.height - 1}, {size.width - 12, 10}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical |
                             LayoutOptionsAlignRight);

    return {size.width, size.height};
};

void ReadableSummaryScreen::drawInRect(Rect rect) {
    leftArrowView_.draw();
    upperLeftTextView_.draw();
    rightArrowView_.draw();
    bottomRightTextView_.draw();
}

void ReadableSummaryScreen::setPlaceInReadable(unsigned long placeInReadable) {
    placeInReadable_ = placeInReadable;
    preservePlace_ = 0;
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

    draw();

    // This is a slight hack to get around the fact that the bounding box of the display is not
    // context specific
    auto oldDisplayBoundingBox = insetBoundingBox();

    if (firstLoad_) {
        unsigned long placeInSummary = 0;
        SpiramString summaryString =
            readable_->summary(placeInReadable_, placeInSummary, showUnread_);

        if (preservePlace_) {
            placeInSummary = preservePlace_;
            preservePlace_ = 0;
        }
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

    resetBoundingBox(oldDisplayBoundingBox);
}

auto ReadableSummaryScreen::insetBoundingBox() -> DisplayRect {
    auto oldDisplayBoundingBox = display_.getBoundingBox();
    auto newDisplayBoundingBox = display_.getBoundingBox();
    newDisplayBoundingBox.y += display_.defaultYPadding;
    newDisplayBoundingBox.height -= 2 * display_.defaultYPadding;
    display_.setBoundingBox(newDisplayBoundingBox);
    return oldDisplayBoundingBox;
}

void ReadableSummaryScreen::resetBoundingBox(DisplayRect oldDisplayBoundingBox) {
    // Reset to the original bounding box for when we return to another screen
    display_.setBoundingBox(oldDisplayBoundingBox);
}

void ReadableSummaryScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        back();
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        showUnread_ = !showUnread_;
        preservePlace_ = prevPlace_;
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
    case RemoteCommand::LEFT: {
        auto oldDisplayBoundingBox = insetBoundingBox();
        newPlace_ = renderer_.getPreviousPlace();
        setNeedsRerender();
        resetBoundingBox(oldDisplayBoundingBox);
        break;
    }
    default:
        break;
    }
}
