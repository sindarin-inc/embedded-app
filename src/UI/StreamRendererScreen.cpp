#include "UI/StreamRendererScreen.hpp"

#include "sindarin-debug.h"

void StreamRendererScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }
    newPlace_ = renderer_->renderPage(newPlace_);
    log_d("%lu is the new place in the text", newPlace_);
    needsRerender = false;
}

void StreamRendererScreen::processCommand(RemoteCommand command) {
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
    case RemoteCommand::NEXT:
        needsRerender = true;
        break;
    case RemoteCommand::PREV:
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
