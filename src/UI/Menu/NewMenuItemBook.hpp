#pragma once

#include <functional>
#include <string>
#include <utility>

#include "Models/Readable.hpp"
#include "NewMenuItem.hpp"

class NewMenuItemBook : public NewMenuItem {
public:
    ~NewMenuItemBook() override = default;
    NewMenuItemBook(std::shared_ptr<Readable> readable, std::function<void()> callback)
        : NewMenuItem(std::move(callback)), readable_(readable) {}

    [[nodiscard]] auto title() const -> const SpiramString & { return readable_->title; }

    [[nodiscard]] auto author() const -> const SpiramString & { return readable_->authors[0].name; }

private:
    std::shared_ptr<Readable> readable_;
};
