#pragma once

#include <functional>
#include <string>
#include <utility>

#include "NewMenuItem.hpp"

class NewMenuItemMultiLine : public NewMenuItem {
public:
    ~NewMenuItemMultiLine() override = default;
    NewMenuItemMultiLine(std::string text, int maxLines, std::function<void()> callback)
        : NewMenuItem(std::move(callback)), text_(std::move(text)), maxLines_(maxLines) {}

    [[nodiscard]] auto id() const -> std::string override { return text_; }

    [[nodiscard]] auto text() const -> const std::string & { return text_; }

    [[nodiscard]] auto maxLines() const -> int { return maxLines_; }

private:
    std::string text_;
    int maxLines_;
};
