#pragma once

#include <functional>

// Base class for a menu item (menu row) model
class NewMenuItem {
public:
    virtual ~NewMenuItem() = default;
    NewMenuItem(std::function<void()> callback) : callback_(std::move(callback)) {}

    virtual void execute() const {
        if (callback_) {
            callback_();
        }
    }

protected:
    std::function<void()> callback_;
};
