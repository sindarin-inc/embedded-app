#include "ReadableSummaryScreen.hpp"

#include "Misc/SpiramAllocator.hpp"

auto ReadableSummaryScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(leftArrowView_, {{0, 1}, {5, 10}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);
    context.setViewFrame(upperLeftTextView_, {{10, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    return {size.width, size.height - 10};
};

void ReadableSummaryScreen::drawInRect(Rect rect) {
    leftArrowView_.draw();
    upperLeftTextView_.draw();
}

void ReadableSummaryScreen::setPlaceInFile(unsigned long placeInFile) {
    placeInFile_ = placeInFile;
}

void ReadableSummaryScreen::render(DisplaySystem &display) {

    setNeedsLayout();

    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    PROFILE_START(ReadableSummaryScreenLayoutAndDraw);
    draw();
    PROFILE_END_LOG_US(ReadableSummaryScreenLayoutAndDraw);

    if (firstLoad_) {
        unsigned long placeInSummary = 0;
        SpiramString summaryString = readable_->summary(placeInFile_, placeInSummary);

        auto stream = std::make_shared<StdStringStream>(summaryString);
        renderer_.setStream(stream);
        newPlace_ = renderer_.renderPage(placeInSummary);
    } else {
        prevPlace_ = newPlace_;
        newPlace_ = renderer_.renderPage(newPlace_);
    }

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
        back();
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
        back();
        setNeedsRerender();
        break;
    default:
        break;
    }
}
