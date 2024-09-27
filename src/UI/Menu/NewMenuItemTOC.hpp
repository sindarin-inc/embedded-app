#pragma once

#include <functional>
#include <string>
#include <utility>

#include "NewMenuItem.hpp"

class NewMenuItemTOC : public NewMenuItem {
public:
    ~NewMenuItemTOC() override = default;
    NewMenuItemTOC(std::string title, std::string place, std::function<void()> callback)
        : NewMenuItem(std::move(callback)), title_(std::move(title)), place_(std::move(place)) {}

    [[nodiscard]] auto title() const -> const std::string & { return title_; }

    [[nodiscard]] auto place() const -> const std::string & { return place_; }

private:
    std::string title_;
    std::string place_;
};
