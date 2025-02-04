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

    void setBackCallback(std::function<void()> callback) { backCallback_ = std::move(callback); }
    virtual void back() {
        if (backCallback_ != nullptr) {
            backCallback_();
        }
        // temp goes out of scope and releases the last reference to the pointer.
    }

protected:
    bool needsRerender_{true};
    bool firstLoad_{true};
    std::function<void()> backCallback_;
};
