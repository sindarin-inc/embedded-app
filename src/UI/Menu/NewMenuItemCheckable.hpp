#pragma once

#include <functional>
#include <string>
#include <utility>

#include "NewMenuItem.hpp"

class NewMenuItemCheckable : public NewMenuItem {
public:
    ~NewMenuItemCheckable() override = default;
    NewMenuItemCheckable(std::string title, bool initialChecked, std::function<void()> callback)
        : NewMenuItem(std::move(callback)), title_(std::move(title)), isChecked_(initialChecked) {}

    [[nodiscard]] auto title() const -> const std::string & { return title_; }

    [[nodiscard]] auto isChecked() const -> bool { return isChecked_; }

    void execute() override {
        isChecked_ = !isChecked_;
        if (callback_) {
            callback_();
        }
    }

private:
    std::string title_;
    bool isChecked_;
};
