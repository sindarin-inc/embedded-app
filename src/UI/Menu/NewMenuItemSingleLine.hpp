#pragma once

#include <functional>
#include <string>

#include "NewMenuItem.hpp"

class NewMenuItemSingleLine : public NewMenuItem {
public:
    NewMenuItemSingleLine(std::string title, std::function<void()> callback)
        : NewMenuItem(callback), title_(std::move(title)) {}

    const std::string &title() const { return title_; }

private:
    std::string title_;
};
