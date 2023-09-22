#include "ReadableDictionaryScreen.hpp"

#include "Misc/SpiramAllocator.hpp"

auto ReadableDictionaryScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(leftArrowView_, {{0, -5}, {10, 5}});
    context.setViewFrame(upperLeftTextView_, {{15, -8}, size});

    return {size.width, size.height};
};

void ReadableDictionaryScreen::drawInRect(Rect rect) {
    leftArrowView_.draw();
    upperLeftTextView_.draw();
}

void ReadableDictionaryScreen::setPlaceInReadable(std::shared_ptr<Place> placeInReadable) {
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

    if (firstLoad_) {
        std::shared_ptr<Place> placeInDictionary = nullptr;
        SpiramString dictionaryString = readable_->definitions(placeInReadable_, placeInDictionary);

        if (preservePlace_) {
            placeInDictionary = preservePlace_;
            preservePlace_ = nullptr;
        }
        auto stream = std::make_shared<StdStringStream>(dictionaryString);
        renderer_.setStream(stream);
        std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(placeInDictionary);
        log_d("Place in readable: %s, place in dictionary: %s", placeInReadable_->str().c_str(),
              placeInDictionary->str().c_str());
    } else {
        prevPlace_ = newPlace_;
        std::tie(newPlace_, darkPicturePercent) = renderer_.renderPage(newPlace_);
    }

    atEnd_ = checkAtEnd();
    display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                          display.darkPictureVComAdj(darkPicturePercent));
    firstLoad_ = false;

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
        newPlace_ = renderer_.getPreviousPlace();
        setNeedsRerender();
        resetBoundingBox(oldDisplayBoundingBox);
        break;
    }
    default:
        break;
    }
}
