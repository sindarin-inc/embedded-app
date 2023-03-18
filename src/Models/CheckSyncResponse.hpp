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
    std::string nextPageUrl;
    int totalCount;

    auto fromJson(const JsonObject &j) -> bool override {
        synced = j["synced"].as<bool>();
        message = j["message"].as<std::string>();
        userBooks.clear();
        userArticles.clear();
        if (!j.containsKey("results")) {
            log_w("Failed to parse CheckSyncResponse %s", DumpJson(j).c_str());
            return false;
        }
        auto results = j["results"].as<JsonObject>();

        if (results.containsKey("books_since")) {
            booksSince = results["books_since"].as<std::string>();
            for (const auto &userReadable : results["books"].as<JsonArray>()) {
                userBooks.emplace_back();
                userBooks.back().fromJson(userReadable);
            }
        } else {
            booksSince = "";
        }

        if (results.containsKey("articles_since")) {
            articlesSince = results["articles_since"].as<std::string>();
            for (const auto &userArticle : results["articles"].as<JsonArray>()) {
                userArticles.emplace_back();
                userArticles.back().fromJson(userArticle);
            }
        } else {
            articlesSince = "";
        }

        if (j.containsKey("next")) {
            nextPageUrl = j["next"].isNull() ? std::string() : j["next"].as<std::string>();
        }
        if (j.containsKey("count")) {
            totalCount = j["count"].as<int>();
        }
        return true;
    }
};
