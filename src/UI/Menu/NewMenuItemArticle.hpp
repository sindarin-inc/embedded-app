#pragma once

#include <functional>
#include <string>
#include <utility>

#include "Models/Readable.hpp"
#include "NewMenuItem.hpp"

class NewMenuItemArticle : public NewMenuItem {
public:
    ~NewMenuItemArticle() override = default;
    // We take in the source now even though we don't use it right now. Planning ahead.
    NewMenuItemArticle(std::shared_ptr<Readable> readable, std::function<void()> callback,
                       bool archived = false)
        : NewMenuItem(std::move(callback)), readable_(std::move(readable)), archived_(archived) {}

    [[nodiscard]] auto title() const -> std::string {
        return StringFormat("%s%s", archived_ ? "Archived: " : "", readable_->title.c_str());
    }

    // The source isn't in in the Readable yet, so we return an empty string for now.
    // [[nodiscard]] auto source() const -> const std::string & { return readable->source; }

private:
    std::shared_ptr<Readable> readable_;
    bool archived_;
};
