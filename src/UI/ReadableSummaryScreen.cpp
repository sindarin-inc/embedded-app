#include "ReadableSummaryScreen.hpp"

#include "Misc/SpiramAllocator.hpp"

auto ReadableSummaryScreen::layout(LayoutContext &context, Size size) -> Size { return size; };

void ReadableSummaryScreen::drawInRect(Rect rect) {}

void ReadableSummaryScreen::render(DisplaySystem &display) {

    setNeedsLayout();

    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    if (firstLoad_) {
        PROFILE_START(ReadableSummaryScreenLayoutAndDraw);
        draw();
        PROFILE_END_LOG_US(ReadableSummaryScreenLayoutAndDraw);
    }

    if (firstLoad_) {
        SpiramString summaryString = readable_->summary();

        auto stream = std::make_shared<StdStringStream>(summaryString);
        renderer_.setStream(stream);
        newPlace_ = renderer_.renderPage(0);
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
