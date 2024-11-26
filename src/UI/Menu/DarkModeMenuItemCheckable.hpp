#pragma once

#include <functional>
#include <string>
#include <utility>

#include "MenuItem.hpp"

class DarkModeMenuItemCheckable : public MenuItem {
public:
    ~DarkModeMenuItemCheckable() override = default;
    DarkModeMenuItemCheckable(std::string title, std::function<void()> callback,
                              std::function<bool()> isCheckedCallback,
                              std::function<void()> rightButtonCallback = nullptr)
        : MenuItem(std::move(callback), std::move(rightButtonCallback)), title_(std::move(title)),
          isCheckedCallback_(std::move(isCheckedCallback)) {}

    [[nodiscard]] auto id() const -> std::string override { return title_; }

    [[nodiscard]] auto title() const -> const std::string & { return title_; }

    [[nodiscard]] auto isChecked() const -> bool { return isCheckedCallback_(); }

private:
    std::string title_;
    std::function<bool()> isCheckedCallback_;
};
