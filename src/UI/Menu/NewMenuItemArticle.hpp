#pragma once

#include <functional>
#include <string>
#include <utility>

#include "NewMenuItem.hpp"

class NewMenuItemArticle : public NewMenuItem {
public:
    ~NewMenuItemArticle() override = default;
    // We take in the source now even though we don't use it right now. Planning ahead.
    NewMenuItemArticle(std::string title, std::string source, std::function<void()> callback)
        : NewMenuItem(std::move(callback)), title_(std::move(title)), source_(std::move(source)) {}

    [[nodiscard]] auto title() const -> const std::string & { return title_; }

    [[nodiscard]] auto source() const -> const std::string & { return source_; }

private:
    std::string title_;
    std::string source_;
};
