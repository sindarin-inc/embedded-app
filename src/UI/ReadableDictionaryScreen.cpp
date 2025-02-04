#include "ReadableDictionaryScreen.hpp"

#include <pugixml.hpp>

#include "Misc/SpiramAllocator.hpp"

auto ReadableDictionaryScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    auto fullFrame = SolRect::fromSize(size);
    auto backgroundFrame = fullFrame.inset(0);

    context.setViewFrame(backgroundView_, backgroundFrame);

    auto contentFrame = backgroundFrame.inset(10, 2);

    context.setViewFrame(dismissLabelView_, contentFrame,
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop);

    return {size.width, size.height};
};

void ReadableDictionaryScreen::drawInRect(SolRect rect) {
    backgroundView_.fillColor = display_.bg;
    backgroundView_.strokeColor = display_.fg;
    backgroundView_.draw();

    dismissLabelView_.draw();
}

void ReadableDictionaryScreen::setPlaceInReadable(std::shared_ptr<EPubPlace> placeInReadable) {
    placeInReadable_ = std::move(placeInReadable);
    preservePlace_ = nullptr;
}

auto ReadableDictionaryScreen::checkAtEnd() -> bool {
    return (newPlace_ == nullptr) || newPlace_->getOffset() == 0;
}

void ReadableDictionaryScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect());

    // Double up on the vertical insets
    auto rect =
        DisplayRect::fromSizeAndInsets(display_.width(), display_.height(), {30, 30, 30, 30});
    rect = rect.inset(10, 10, 15, 15);
    renderer_.setRect(rect);

    draw();

    uint8_t darkPicturePercent = 0;

    if (loadedChapterHref_ != placeInReadable_->getHRef()) {
        std::shared_ptr<TxtPlace> placeInDictionary = nullptr;
        rendererStream_ = renderer_.getStream();
        if (rendererStream_ == nullptr) {
            rendererStream_ = renderer_.getStream();
        }
        auto epubStream = std::dynamic_pointer_cast<EPubFile>(rendererStream_);
        int currentOffset = placeInReadable_->getOffset();
        log_d("Loading dictionary for place: %s : %d", placeInReadable_->str().c_str(),
              currentOffset);
        auto dictionaryString =
            epubStream->getDictionaryContent(placeInReadable_, placeInDictionary);

        if (preservePlace_) {
            placeInDictionary = preservePlace_;
            preservePlace_ = nullptr;
        }

        auto stream = std::make_shared<StdStringStream>(dictionaryString);
        renderer_.setStream(stream);
        if (placeInDictionary == nullptr) {
            placeInDictionary = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));
        }
        std::shared_ptr<Place> newPlace;
        std::tie(newPlace, darkPicturePercent) = renderer_.renderPage(placeInDictionary);
        newPlace_ = std::dynamic_pointer_cast<TxtPlace>(newPlace);
        log_d("Place in readable: %s, place in dictionary: %s. Next place: %" PRIu32,
              placeInReadable_->str().c_str(), placeInDictionary->str().c_str(),
              newPlace_ != nullptr ? newPlace_->getOffset() : -1);
    } else {
        prevPlace_ = newPlace_;
        std::shared_ptr<Place> newPlace;
        std::tie(newPlace, darkPicturePercent) = renderer_.renderPage(newPlace_);
        newPlace_ = std::dynamic_pointer_cast<TxtPlace>(newPlace);
    }

    atEnd_ = checkAtEnd();
    if (prefs_.fastTransitions.get().value_or(false)) {
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                              display.darkPictureVComAdj(darkPicturePercent));
    } else {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                              display.darkPictureVComAdj(darkPicturePercent));
    }
    loadedChapterHref_ = placeInReadable_->getHRef();

    // Restore the renderer's rect
    renderer_.setRect(GetTextRectFromPrefs(display, prefs_));
}

void ReadableDictionaryScreen::restoreRendererStream() {
    renderer_.setStream(rendererStream_);
    rendererStream_ = nullptr;
}

void ReadableDictionaryScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        restoreRendererStream();
        setNeedsRerender();
        back();
        return;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        if (atEnd_) {
            restoreRendererStream();
            setNeedsRerender();
            back();
            return;
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
