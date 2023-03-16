#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/UserReadable.hpp"

class CheckSyncResponse : JsonSerializable {
public:
    CheckSyncResponse() = default;
    ~CheckSyncResponse() override = default;

    bool synced;
    std::string message;
    std::vector<UserReadable, SpiramAllocator<UserReadable>> userBooks;
    std::vector<UserReadable, SpiramAllocator<UserReadable>> userArticles;
    std::string booksSince;
    std::string articlesSince;

    auto fromJson(const JsonObject &j) -> bool override {
        synced = j["synced"].as<bool>();
        message = j["message"].as<std::string>();
        userBooks.clear();
        userArticles.clear();

        if (j.containsKey("books_since")) {
            booksSince = j["books_since"].as<std::string>();
            for (const auto &userReadable : j["books"].as<JsonArray>()) {
                userBooks.emplace_back();
                userBooks.back().fromJson(userReadable);
            }
        } else {
            booksSince = "";
        }

        if (j.containsKey("articles_since")) {
            articlesSince = j["articles_since"].as<std::string>();
            for (const auto &userArticle : j["articles"].as<JsonArray>()) {
                userArticles.emplace_back();
                userArticles.back().fromJson(userArticle);
            }
        } else {
            articlesSince = "";
        }

        return true;
    }
};
