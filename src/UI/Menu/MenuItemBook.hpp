#pragma once

#include <functional>
#include <string>
#include <utility>

#include "MenuItem.hpp"
#include "Models/Readable.hpp"

class MenuItemBook : public MenuItem {
public:
    ~MenuItemBook() override = default;
    MenuItemBook(std::shared_ptr<Readable> readable, std::function<void()> callback,
                 bool archived = false)
        : MenuItem(std::move(callback)), readable_(std::move(std::move(readable))),
          archived_(archived) {}

    [[nodiscard]] auto id() const -> std::string override {
        return readable_->shortenedUuid.c_str();
    }

    [[nodiscard]] auto title() const -> std::string {
        return StringFormat("%s%s", archived_ ? "Archived: " : "", readable_->title.c_str());
    }

    [[nodiscard]] auto author() const -> SpiramString { return readable_->authorString(); };

private:
    std::shared_ptr<Readable> readable_;
    bool archived_;
};
