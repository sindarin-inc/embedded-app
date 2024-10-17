#pragma once

#include <functional>
#include <string>
#include <utility>

#include "NewMenuItem.hpp"

class NewMenuItemSingleLine : public NewMenuItem {
public:
    ~NewMenuItemSingleLine() override = default;
    NewMenuItemSingleLine(std::string title, std::function<void()> callback)
        : NewMenuItem(std::move(callback)), title_(std::move(title)) {}

    [[nodiscard]] auto id() const -> std::string override { return title_; }

    [[nodiscard]] auto title() const -> const std::string & { return title_; }

private:
    std::string title_;
};
