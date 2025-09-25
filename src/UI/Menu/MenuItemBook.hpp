#pragma once

#include <functional>
#include <string>
#include <utility>

#include "MenuItem.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/KindleBook.hpp"
#include "Models/Readable.hpp"

class MenuItemBook : public MenuItem {
public:
    ~MenuItemBook() override = default;
    MenuItemBook(std::shared_ptr<Readable> readable, std::function<void()> callback,
                 bool archived = false)
        : MenuItem(std::move(callback)), readable_(std::move(readable)), archived_(archived) {}

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

// Special menu item for Kindle books
class MenuItemKindleBook : public MenuItem {
public:
    ~MenuItemKindleBook() override = default;
    MenuItemKindleBook(const KindleBook &kindleBook, std::function<void()> callback,
                       bool archived = false, int currentPage = -1)
        : MenuItem(std::move(callback)), kindleBook_(kindleBook), archived_(archived),
          currentPage_(currentPage) {}

    [[nodiscard]] auto id() const -> std::string override {
        // Use the shortened ID for a unique but compact identifier
        return {kindleBook_.shortenedUuid.c_str()};
    }

    [[nodiscard]] auto title() const -> std::string {
        return StringFormat("%s%s", archived_ ? "Archived: " : "", kindleBook_.title.c_str());
    }

    [[nodiscard]] auto author() const -> SpiramString { return kindleBook_.author; };

    [[nodiscard]] auto progressPercent() const -> int {
        // Calculate percentage on demand from current page and total pages
        if (currentPage_ > 0 && kindleBook_.progressTotalPages > 0) {
            return (currentPage_ * 100) / kindleBook_.progressTotalPages;
        }
        return 0;
    }

private:
    KindleBook kindleBook_;
    bool archived_;
    int currentPage_;
};
