#pragma once

#include <functional>
#include <string>

// Base class for a menu item (menu row) model
class MenuItem {
public:
    virtual ~MenuItem() = default;
    MenuItem(std::function<void()> callback, std::function<void()> submenuCallback = nullptr)
        : callback_(std::move(callback)), submenuCallback_(std::move(submenuCallback)) {}

    [[nodiscard]] virtual auto id() const -> std::string = 0;
    virtual void execute() {
        if (callback_) {
            callback_();
        }
    }
    virtual void executeSubmenu() {
        if (submenuCallback_) {
            submenuCallback_();
        }
    }

protected:
    std::function<void()> callback_;
    std::function<void()> submenuCallback_;
};
