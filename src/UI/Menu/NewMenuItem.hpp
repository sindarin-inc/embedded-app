#pragma once

#include <functional>
#include <string>

// Base class for a menu item (menu row) model
class NewMenuItem {
public:
    virtual ~NewMenuItem() = default;
    NewMenuItem(std::function<void()> callback) : callback_(std::move(callback)) {}

    [[nodiscard]] virtual auto id() const -> std::string = 0;
    virtual void execute() {
        if (callback_) {
            callback_();
        }
    }

protected:
    std::function<void()> callback_;
};
