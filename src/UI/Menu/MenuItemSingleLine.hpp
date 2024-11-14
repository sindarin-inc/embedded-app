#pragma once

#include <functional>
#include <string>
#include <utility>

#include "MenuItem.hpp"

class MenuItemSingleLine : public MenuItem {
public:
    ~MenuItemSingleLine() override = default;
    MenuItemSingleLine(std::string title, std::function<void()> callback)
        : MenuItem(std::move(callback)), title_(std::move(title)) {}

    [[nodiscard]] auto id() const -> std::string override { return title_; }

    [[nodiscard]] auto title() const -> const std::string & { return title_; }

private:
    std::string title_;
};
