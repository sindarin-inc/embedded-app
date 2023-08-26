#include "ReadableDictionaryScreen.hpp"

#include "Misc/SpiramAllocator.hpp"

auto ReadableDictionaryScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(leftArrowView_, {{0, -5}, {10, 5}});
    context.setViewFrame(upperLeftTextView_, {{15, -8}, size});

    return {size.width, size.height - 10};
};

void ReadableDictionaryScreen::drawInRect(Rect rect) {
    leftArrowView_.draw();
    upperLeftTextView_.draw();
}

void ReadableDictionaryScreen::setPlaceInReadable(unsigned long placeInReadable) {
    placeInReadable_ = placeInReadable;
    preservePlace_ = 0;
}

auto ReadableDictionaryScreen::checkAtEnd() -> bool { return (newPlace_ == 0); }

void ReadableDictionaryScreen::render(DisplaySystem &display) {
    setNeedsLayout();

    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    draw();

    // This is a slight hack to get around the fact that the bounding box of the display is not
    // context specific
    if (oldDisplayBoundingBox_.height == 0) {
        oldDisplayBoundingBox_ = display_.getBoundingBox();
        auto newDisplayBoundingBox = display_.getBoundingBox();
        newDisplayBoundingBox.y += display.defaultYPadding;
        newDisplayBoundingBox.height -= 2 * display.defaultYPadding;
        display_.setBoundingBox(newDisplayBoundingBox);
    }

    if (firstLoad_) {
        unsigned long placeInDictionary = 0;
        SpiramString dictionaryString =
            readable_->definitions(placeInReadable_, &placeInDictionary);

        if (preservePlace_) {
            placeInDictionary = preservePlace_;
            preservePlace_ = 0;
        }
        auto stream = std::make_shared<StdStringStream>(dictionaryString);
        renderer_.setStream(stream);
        newPlace_ = renderer_.renderPage(placeInDictionary);
        log_d("Place in readable: %lu, place in dictionary: %lu", placeInReadable_,
              placeInDictionary);
    } else {
        prevPlace_ = newPlace_;
        newPlace_ = renderer_.renderPage(newPlace_);
    }

    atEnd_ = checkAtEnd();
    display.updateDisplay(DisplaySystem::UpdateType::FULL);
    firstLoad_ = false;
}

void ReadableDictionaryScreen::resetBoundingBox() {
    // Reset to the original bounding box for when we return to another screen
    display_.setBoundingBox(oldDisplayBoundingBox_);
    oldDisplayBoundingBox_ = {};
}

void ReadableDictionaryScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        resetBoundingBox();
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
            resetBoundingBox();
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
