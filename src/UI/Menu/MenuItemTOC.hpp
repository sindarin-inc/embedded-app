#pragma once

#include <functional>
#include <string>
#include <utility>

#include "MenuItem.hpp"

class MenuItemTOC : public MenuItem {
public:
    ~MenuItemTOC() override = default;
    MenuItemTOC(std::string title, std::string place, std::function<void()> callback)
        : MenuItem(std::move(callback)), title_(std::move(title)), place_(std::move(place)) {}

    [[nodiscard]] auto id() const -> std::string override { return title_; }

    [[nodiscard]] auto title() const -> const std::string & { return title_; }

    [[nodiscard]] auto place() const -> const std::string & { return place_; }

private:
    std::string title_;
    std::string place_;
};
