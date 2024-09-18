#pragma once

#include <functional>

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"

/*!
Abstract base class for subclasses that represent a piece of content that takes over the whole
screen.
*/
class Screen {
public:
    virtual ~Screen() = default;
    virtual void render(DisplaySystem &display) = 0;
    virtual void processCommand(RemoteCommand command) = 0;
    virtual void setNeedsRerender() { needsRerender_ = true; }
    virtual void setFirstLoad() { firstLoad_ = true; }
    virtual auto needsRerender() -> bool { return needsRerender_; }
    virtual void clearNeedsRerender() { needsRerender_ = false; }

    // Subclasses must implement this to show a temporary message that can be displayed
    // from their screen. This can't do it here because the MsgPopup is a subclass of
    // this class.
    virtual void showTemporaryMessage(DisplaySystem &display, const std::string &message,
                                      int duration = 2000, bool rerenderAfter = false){};

    virtual void loop(DisplaySystem &display, RemoteCommand command) {
        if (command != RemoteCommand::NONE) {
            processCommand(command);
        }
        if (needsRerender()) {
            render(display);
            clearNeedsRerender();
        }
    }

    void setBackCallback(std::function<void()> callback) { backCallback_ = std::move(callback); }
    virtual void back() {
        if (backCallback_ != nullptr) {
            backCallback_();
        }
    }

protected:
    bool needsRerender_{true};
    bool firstLoad_{true};
    std::function<void()> backCallback_;
};
