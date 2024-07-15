#include "ReadableSummaryScreen.hpp"

#include <utility>

#include "Misc/SpiramAllocator.hpp"
#include "Models/UserReadablePartialChapterSummary.hpp"

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

void ReadableSummaryScreen::setPlaceInReadable(std::shared_ptr<EPubPlace> placeInReadable) {
    placeInReadable_ = std::move(placeInReadable);
    preservePlace_ = nullptr;
}

auto ReadableSummaryScreen::checkAtEnd() -> bool {
    return (newPlace_ == nullptr) || newPlace_->getOffset() == 0;
}

void ReadableSummaryScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect());

    if (showUnread_) {
        bottomRightText_ = "Hide Unread Chapters";
    } else {
        bottomRightText_ = "Show Unread Chapters";
    }

    bottomRightTextView_.setText(bottomRightText_);
    rightArrowView_.setNeedsLayout();

    draw();

    // Double up on the vertical insets
    // TODO(john): I'm not sure we want to use the bounding box setting here, but it's what we were
    // doing previously so I'm keeping it for now.
    auto rect = GetTextRectFromPrefs(display, prefs_);
    rect = rect.inset(0, 0, display.defaultTextInsets().top, display.defaultTextInsets().bottom);
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
        newPlace_ = std::dynamic_pointer_cast<TxtPlace>(renderer_.getPreviousPlace());
        setNeedsRerender();
        break;
    }
    default:
        break;
    }
}
