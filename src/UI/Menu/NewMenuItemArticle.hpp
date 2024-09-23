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
    NewMenuItemArticle(std::shared_ptr<Readable> readable, std::function<void()> callback)
        : NewMenuItem(std::move(callback)), readable_(std::move(std::move(readable))) {}

    [[nodiscard]] auto title() const -> const SpiramString & { return readable_->title; }

    // The source isn't in in the Readable yet, so we return an empty string for now.
    // [[nodiscard]] auto source() const -> const std::string & { return readable->source; }

private:
    std::shared_ptr<Readable> readable_;
};
