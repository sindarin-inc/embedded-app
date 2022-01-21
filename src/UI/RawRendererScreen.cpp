#include "UI/RawRendererScreen.hpp"

#include "sindarin-debug.h"

void RawRendererScreen::render() {
    if (!needsRerender) {
        return;
    }
    log_d("Rendering some text in READ MODE");
    newPlace_ = renderer_->renderPageWithBox(newPlace_);
    log_d("%d is the new place in the text", newPlace_);
    needsRerender = false;
}

void RawRendererScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS: {
        newPlace_ = prevPlace_;
        needsRerender = true;
        break;
    }
    case RemoteCommand::CHANNEL_PLUS: {
        prevPlace_ = newPlace_;
        needsRerender = true;
        break;
    }
    case RemoteCommand::PREV:
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        needsRerender = true;
        break;
    case RemoteCommand::BACK:
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    case RemoteCommand::NOP_BUTTON:
        needsRerender = true;
        break;
    default:
        break;
    }
}
