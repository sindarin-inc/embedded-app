#pragma once

#include <ArduinoJson.h>
#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Readable.hpp"
#include "Models/ReadableArchive.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Models/ReadablePlacemark.hpp"
#include "Models/UserReadable.hpp"
#include "Storage/PreferencesStore.hpp"

enum class ReadablesModelType { Article, Book };

auto ReadableModelTypeToString(ReadablesModelType type) -> std::string;

using StoredUuidsVector = SpiramVector<std::string>;

class CheckSyncRequest : JsonSerializable {
public:
    CheckSyncRequest() = default;
    CheckSyncRequest(const CheckSyncRequest &) = delete;
    ~CheckSyncRequest() override = default;

    StoredUuidsVector storedUuids;
    ReadableBookmarksVector bookmarks;
    ReadableArchivesVector archives;
    ReadablePlacemarksVector placemarks;

    std::string since;
    ReadablesModelType modelType;
    bool skipContent;
    SyncReason syncReason;
    uint8_t ptBattery;
    std::string ptBuild;
    std::string build;
    std::string gitHash;
    std::string sn;

    auto fromJson(const JsonObject &j) -> bool override { return true; }
};

auto ConvertToJson(const CheckSyncRequest &src, JsonVariant dst) -> bool;

class CheckSyncResponse : JsonSerializable {
public:
    CheckSyncResponse() = default;
    CheckSyncResponse(const CheckSyncResponse &) = delete;
    ~CheckSyncResponse() override = default;

    bool synced;
    std::string message;
    std::vector<UserReadable, SpiramAllocator<UserReadable>> userBooks;
    std::vector<UserReadable, SpiramAllocator<UserReadable>> userArticles;
    std::string booksSince;
    std::string articlesSince;
    std::string nextPageUrl;
    int totalCount;
    std::string timezone;

    auto fromJson(const JsonObject &j) -> bool override {
        message = j["message"].as<std::string>();
        userBooks.clear();
        userArticles.clear();
        if (!j.containsKey("results")) {
            log_w("Failed to parse CheckSyncResponse %s", DumpJson(j).c_str());
            return false;
        }
        auto results = j["results"].as<JsonObject>();
        synced = results["synced"].as<bool>();
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

        if (results.containsKey("timezone")) {
            timezone = results["timezone"].as<std::string>();
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
