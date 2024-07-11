#include "ReadableSummaryScreen.hpp"

#include <utility>

#include "Misc/SpiramAllocator.hpp"
#include "Models/UserReadablePartialChapterSummary.hpp"

auto ReadableSummaryScreen::layout(LayoutContext &context, Size size) -> Size {
    auto fullFrame = Rect::fromSize(size);
    auto backgroundFrame = fullFrame.inset(0);

    context.setViewFrame(backgroundView_, backgroundFrame);

    auto contentFrame = backgroundFrame.inset(10);
    context.setViewFrame(leftArrowView_,
                         {{contentFrame.origin.x, contentFrame.origin.y - 5}, {10, 5}});
    context.setViewFrame(upperLeftTextView_,
                         {{contentFrame.origin.x + 15, contentFrame.origin.y - 8},
                          {contentFrame.size.width, contentFrame.size.height}});
    context.setViewSizeInRect(rightArrowView_, {10, 5},
                              {{contentFrame.origin.x, 7}, contentFrame.size},
                              LayoutOptionsAlignBottom | LayoutOptionsAlignRight);
    context.setViewFrame(
        bottomRightTextView_,
        {{contentFrame.origin.x, contentFrame.origin.y + contentFrame.size.height - 11},
         {contentFrame.size.width - 12, 10}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical | LayoutOptionsAlignRight);

    contentFrame_ = contentFrame;

    return size;
}

void ReadableSummaryScreen::drawInRect(Rect rect) {
    backgroundView_.fillColor = display_.bg;
    backgroundView_.strokeColor = display_.fg;
    backgroundView_.draw();

    leftArrowView_.draw();
    upperLeftTextView_.draw();
    rightArrowView_.draw();
    bottomRightTextView_.draw();
}

void ReadableSummaryScreen::setPlaceInReadable(std::shared_ptr<EPubPlace> placeInReadable) {
    placeInReadable_ = std::move(placeInReadable);
    preservePlace_ = nullptr;
}

auto ReadableSummaryScreen::checkAtEnd() -> bool {
    return (newPlace_ == nullptr) || newPlace_->getOffset() == 0;
}

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

    resetBoundingBox(oldDisplayBoundingBox);
}

auto ReadableSummaryScreen::insetBoundingBox() -> DisplayRect {
    auto oldDisplayBoundingBox = display_.getBoundingBox();
    auto newDisplayBoundingBox = DisplayRect{static_cast<int>(contentFrame_.origin.x),
                                             static_cast<int>(contentFrame_.origin.y),
                                             static_cast<uint32_t>(contentFrame_.size.width),
                                             static_cast<uint32_t>(contentFrame_.size.height)};
    // newDisplayBoundingBox.y += 2 * contentFrame_.origin.y;
    // newDisplayBoundingBox.height -= 2 * contentFrame_.origin.y;
    // newDisplayBoundingBox.x += 2 * contentFrame_.origin.x;
    // newDisplayBoundingBox.width -= contentFrame_.origin.x;
    display_.setBoundingBox(newDisplayBoundingBox);
    return oldDisplayBoundingBox;
}

void ReadableSummaryScreen::resetBoundingBox(DisplayRect oldDisplayBoundingBox) {
    // Reset to the original bounding box for when we return to another screen
    display_.setBoundingBox(oldDisplayBoundingBox);
}

void ReadableSummaryScreen::restoreRendererStream() {
    renderer_.setStream(rendererStream_);
    rendererStream_ = nullptr;
}

void ReadableSummaryScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        restoreRendererStream();
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
            restoreRendererStream();
            back();
        }

        setNeedsRerender();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT: {
        auto oldDisplayBoundingBox = insetBoundingBox();
        newPlace_ = std::dynamic_pointer_cast<TxtPlace>(renderer_.getPreviousPlace());
        setNeedsRerender();
        resetBoundingBox(oldDisplayBoundingBox);
        break;
    }
    default:
        break;
    }
}
