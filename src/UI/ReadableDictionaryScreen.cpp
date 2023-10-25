#include "ReadableDictionaryScreen.hpp"

#include "Misc/SpiramAllocator.hpp"
#include "pugixml.hpp"

auto ReadableDictionaryScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(leftArrowView_, {{0, -5}, {10, 5}});
    context.setViewFrame(upperLeftTextView_, {{15, -8}, size});

    return {size.width, size.height};
};

void ReadableDictionaryScreen::drawInRect(Rect rect) {
    leftArrowView_.draw();
    upperLeftTextView_.draw();
}

void ReadableDictionaryScreen::setPlaceInReadable(std::shared_ptr<EPubPlace> placeInReadable) {
    placeInReadable_ = std::move(placeInReadable);
    preservePlace_ = nullptr;
}

auto ReadableDictionaryScreen::checkAtEnd() -> bool { return (newPlace_ == nullptr); }

void ReadableDictionaryScreen::render(DisplaySystem &display) {
    setNeedsLayout();

    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    draw();

    // This is a slight hack to get around the fact that the bounding box of the display is not
    // context specific
    auto oldDisplayBoundingBox = insetBoundingBox();
    uint8_t darkPicturePercent = 0;

    if (loadedChapterId_ != placeInReadable_->getHRef()) {
        std::shared_ptr<Place> placeInDictionary = nullptr;
        auto rendererStream = epubRenderer_.getStream();
        auto epubStream = std::dynamic_pointer_cast<EPubFile>(rendererStream);
        auto chapterId = epubStream->getIdByHref(placeInReadable_->getHRef());
        int currentOffset = placeInReadable_->getOffset();
        log_d("Loading dictionary for chapter: %s : %d", chapterId.c_str(), currentOffset);
        auto dictionaryString =
            epubStream->getDictionaryContent(chapterId, placeInReadable_, placeInDictionary);

        if (preservePlace_) {
            placeInDictionary = preservePlace_;
            preservePlace_ = nullptr;
        }

        auto stream = std::make_shared<StdStringStream>(dictionaryString);
        renderer_.setStream(stream);
        if (placeInDictionary == nullptr) {
            placeInDictionary = Place::factory(0);
        }
        auto resultPair = renderer_.renderPage(placeInDictionary);
        newPlace_ = std::dynamic_pointer_cast<TxtPlace>(resultPair.first);
        darkPicturePercent = resultPair.second;
        log_d("Place in readable: %s, place in dictionary: %s. Next place: %d",
              placeInReadable_->str().c_str(), placeInDictionary->str().c_str(),
              newPlace_ != nullptr ? newPlace_->getOffset() : -1);
    } else {
        prevPlace_ = newPlace_;
        auto resultPair = renderer_.renderPage(newPlace_);
        newPlace_ = std::dynamic_pointer_cast<TxtPlace>(resultPair.first);
        darkPicturePercent = resultPair.second;
    }

    atEnd_ = checkAtEnd();
    display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                          display.darkPictureVComAdj(darkPicturePercent));
    loadedChapterId_ = placeInReadable_->getHRef();

    resetBoundingBox(oldDisplayBoundingBox);
}

auto ReadableDictionaryScreen::insetBoundingBox() -> DisplayRect {
    auto oldDisplayBoundingBox = display_.getBoundingBox();
    auto newDisplayBoundingBox = display_.getBoundingBox();
    newDisplayBoundingBox.y += display_.defaultYPadding;
    newDisplayBoundingBox.height -= 2 * display_.defaultYPadding;
    display_.setBoundingBox(newDisplayBoundingBox);
    return oldDisplayBoundingBox;
}

void ReadableDictionaryScreen::resetBoundingBox(DisplayRect oldDisplayBoundingBox) {
    // Reset to the original bounding box for when we return to another screen
    display_.setBoundingBox(oldDisplayBoundingBox);
}

void ReadableDictionaryScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        back();
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
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
        newPlace_ = std::dynamic_pointer_cast<TxtPlace>(renderer_.getPreviousPlace());
        setNeedsRerender();
        resetBoundingBox(oldDisplayBoundingBox);
        break;
    }
    default:
        break;
    }
}
