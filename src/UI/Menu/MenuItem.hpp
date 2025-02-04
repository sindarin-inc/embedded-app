#pragma once

#include <functional>
#include <string>

// Base class for a menu item (menu row) model
class MenuItem {
public:
    virtual ~MenuItem() = default;
    MenuItem(std::function<void()> callback, std::function<void()> rightButtonCallback = nullptr)
        : callback_(std::move(callback)), rightButtonCallback_(std::move(rightButtonCallback)) {}

    [[nodiscard]] virtual auto id() const -> std::string = 0;
    virtual void execute() {
        if (callback_) {
            callback_();
        }
    }
    virtual void executeRightButton() {
        if (rightButtonCallback_ && rightButtonCallback_ != nullptr) {
            rightButtonCallback_();
        }
    }
    auto hasRightButtonCallback() -> bool { return rightButtonCallback_ != nullptr; }

protected:
    std::function<void()> callback_;
    std::function<void()> rightButtonCallback_;
};
