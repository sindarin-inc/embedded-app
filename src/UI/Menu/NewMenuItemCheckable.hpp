#pragma once

#include <functional>
#include <string>
#include <utility>

#include "NewMenuItem.hpp"

class NewMenuItemCheckable : public NewMenuItem {
public:
    ~NewMenuItemCheckable() override = default;
    NewMenuItemCheckable(std::string title, std::function<void()> callback,
                         std::function<bool()> isCheckedCallback)
        : NewMenuItem(std::move(callback)), title_(std::move(title)),
          isCheckedCallback_(std::move(isCheckedCallback)) {}

    [[nodiscard]] auto id() const -> std::string override { return title_; }

    [[nodiscard]] auto title() const -> const std::string & { return title_; }

    [[nodiscard]] auto isChecked() const -> bool { return isCheckedCallback_(); }

private:
    std::string title_;
    std::function<bool()> isCheckedCallback_;
};
