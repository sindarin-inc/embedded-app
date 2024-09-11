#pragma once

#include <functional>
#include <string>
#include <utility>

#include "NewMenuItem.hpp"

class NewMenuItemBook : public NewMenuItem {
public:
    ~NewMenuItemBook() override = default;
    NewMenuItemBook(std::string title, std::string author, std::function<void()> callback)
        : NewMenuItem(std::move(callback)), title_(std::move(title)), author_(std::move(author)) {}

    [[nodiscard]] auto title() const -> const std::string & { return title_; }

    [[nodiscard]] auto author() const -> const std::string & { return author_; }

private:
    std::string title_;
    std::string author_;
};
