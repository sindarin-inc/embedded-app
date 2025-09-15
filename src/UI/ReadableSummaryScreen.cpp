#include "ReadableSummaryScreen.hpp"

#include <utility>

#include "Misc/SpiramAllocator.hpp"
#include "Models/UserReadablePartialChapterSummary.hpp"

#define HEIGHT_EVEN(x) ((((x) % 2 == 0) ? (x) : ((x) + 1)))

auto ReadableSummaryScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    auto fullFrame = SolRect::fromSize(size);
    auto backgroundFrame = fullFrame.inset(0);

    context.setViewFrame(backgroundView_, backgroundFrame);

    auto contentFrame = backgroundFrame.inset(10, 10, 2, 3);

    context.setViewFrame(dismissLabelView_, contentFrame,
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop);

    context.setViewFrame(showUnreadLabelView_, contentFrame,
                         LayoutOptionsSizeToFit | LayoutOptionsAlignRight |
                             LayoutOptionsAlignBottom);

    return size;
}

void ReadableSummaryScreen::drawInRect(SolRect rect) {
    backgroundView_.fillColor = display_.bg;
    backgroundView_.strokeColor = display_.fg;
    backgroundView_.draw();

    dismissLabelView_.draw();
    showUnreadLabelView_.draw();
}

void ReadableSummaryScreen::setPlaceInReadable(std::shared_ptr<EPubPlace> placeInReadable) {
    placeInReadable_ = std::move(placeInReadable);
    preservePlace_ = nullptr;
}

auto ReadableSummaryScreen::checkAtEnd() -> bool {
    return (newPlace_ == nullptr) || renderer_.atEndOfFile();
}

void ReadableSummaryScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect());

    if (showUnread_) {
        showUnreadLabelView_.setText("Hide Unread Chapters");
    } else {
        showUnreadLabelView_.setText("Show Unread Chapters");
    }

    draw();

    // Double up on the vertical insets
    auto rect =
        DisplayRect::fromSizeAndInsets(display_.width(), display_.height(), {30, 30, 30, 30});
    rect = rect.inset(10, 10, 15, 15);
    renderer_.setRect(rect);

    uint8_t darkPicturePercent = 0;

    if (firstLoad_) {
        std::shared_ptr<TxtPlace> placeInSummary =
            std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));
        if (rendererStream_ == nullptr) {
            rendererStream_ = renderer_.getStream();
        }
        auto epubStream = std::dynamic_pointer_cast<EPubFile>(rendererStream_);

        auto partialChapterSummary =
            UserReadablePartialChapterSummary::loadForReadable(fileSystem_, readable_);
        auto summaryString = epubStream->getSummaryContent(placeInReadable_, placeInSummary,
                                                           partialChapterSummary, showUnread_);

        if (preservePlace_) {
            placeInSummary = preservePlace_;
            preservePlace_ = nullptr;
        }
        auto stream = std::make_shared<StdStringStream>(summaryString, DocType::TXT);
        renderer_.setStream(stream);
        std::shared_ptr<Place> newPlace;
        std::tie(newPlace, darkPicturePercent) = renderer_.renderPage(placeInSummary);
        newPlace_ = std::dynamic_pointer_cast<TxtPlace>(newPlace);
        if (placeInSummary != nullptr) {
            log_d("Place in readable: %s, place in summary: %s", placeInReadable_->str().c_str(),
                  placeInSummary->str().c_str());
        }

        // Log telemetry for summary screen opening
        telemetry_.logMetricData(
            TelemetryField::METRIC_DIALOG,
            {{"dialog", std::string("summary")}, {"action", std::string("opened")}});
    } else {
        prevPlace_ = newPlace_;
        std::shared_ptr<Place> newPlace;
        std::tie(newPlace, darkPicturePercent) = renderer_.renderPage(newPlace_);
        newPlace_ = std::dynamic_pointer_cast<TxtPlace>(newPlace);
    }

    atEnd_ = checkAtEnd();
    log_d("Rendering summary with newPlace: %s %s", newPlace_->str().c_str(),
          atEnd_ ? "(at EOF)" : "(not at EOF)");
    if (prefs_.fastTransitions.get().value_or(false)) {
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                              display.darkPictureVComAdj(darkPicturePercent));
    } else {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                              display.darkPictureVComAdj(darkPicturePercent));
    }
    firstLoad_ = false;

    // Restore the renderer's rect
    renderer_.setRect(GetTextRectFromPrefs(display, prefs_));
}

void ReadableSummaryScreen::restoreRendererStream() {
    renderer_.setStream(rendererStream_);
    rendererStream_ = nullptr;
}

void ReadableSummaryScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        restoreRendererStream();
        setNeedsRerender();
        telemetry_.logMetricData(
            TelemetryField::METRIC_DIALOG,
            {{"dialog", std::string("summary")}, {"action", std::string("dismissed")}});
        back();
        return;
    case RemoteCommand::DOWN:
        showUnread_ = !showUnread_;
        preservePlace_ = prevPlace_;
        setFirstLoad();
        telemetry_.logMetricData(
            TelemetryField::METRIC_DIALOG,
            {{"dialog", std::string("summary")}, {"action", std::string("dismissed-at-end")}});
        setNeedsRerender();
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        if (atEnd_) {
            restoreRendererStream();
            setNeedsRerender();
            telemetry_.logMetricData(
                TelemetryField::METRIC_DIALOG,
                {{"dialog", std::string("summary")}, {"action", std::string("dismissed-at-end")}});
            back();
            return;
        }

        telemetry_.logMetricData(
            TelemetryField::METRIC_DIALOG,
            {{"dialog", std::string("summary")}, {"action", std::string("page-forward")}});
        setNeedsRerender();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT: {
        newPlace_ = std::dynamic_pointer_cast<TxtPlace>(renderer_.getPreviousPlace());
        telemetry_.logMetricData(
            TelemetryField::METRIC_DIALOG,
            {{"dialog", std::string("summary")}, {"action", std::string("page-backward")}});
        setNeedsRerender();
        break;
    }
    default:
        break;
    }
}
