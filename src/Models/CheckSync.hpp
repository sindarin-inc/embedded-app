#pragma once

#include <ArduinoJson.h>
#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/BookQuestion.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/KindleBook.hpp"
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
    ReadablesModelType modelType = ReadablesModelType::Book;
    bool skipToc{};
    bool skipContent{};
    bool skipSummary{};
    SyncReason syncReason = SyncReason::Manual;
    uint8_t ptBattery{};
    uint8_t battery{};
    std::string ptBuild;
    std::string build;
    std::string gitHash;
    std::string sn;
    int bookQuestionsVersion{};
    std::string kindleLastSynced;

    auto fromJson(const JsonObject &j) -> bool override { return true; }
};

auto ConvertToJson(const CheckSyncRequest &src, JsonVariant dst) -> bool;

class CheckSyncResponse : JsonSerializable {
public:
    CheckSyncResponse() = default;
    CheckSyncResponse(const CheckSyncResponse &) = delete;
    ~CheckSyncResponse() override = default;

    bool synced{};
    SpiramString message;
    SpiramVector<UserReadable> userBooks;
    SpiramVector<UserReadable> userArticles;
    SpiramString booksSince;
    SpiramString articlesSince;
    SpiramString kindleLastSynced;
    SpiramString nextPageUrl;
    int totalCount{};
    std::string timezone;
    std::string email;
    BookQuestions bookQuestions;
    bool hasBookQuestions{false};
    KindleBooksVector kindleBooks;
    bool hasKindleBooks{false};

    auto fromJson(const JsonObject &j) -> bool override {
        message = j["message"].as<std::string>();
        userBooks.clear();
        userArticles.clear();
        kindleBooks.clear();

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

        // Parse Kindle books if present
        hasKindleBooks = false;
        if (results.containsKey("kindle_books")) {
            // Handle kindle_last_synced field
            if (results.containsKey("kindle_last_synced")) {
                kindleLastSynced = results["kindle_last_synced"].as<std::string>();
            } else {
                kindleLastSynced = "";
            }

            hasKindleBooks = true;

            for (const auto &kindleBookJson : results["kindle_books"].as<JsonArray>()) {
                kindleBooks.emplace_back();
                kindleBooks.back().fromJson(kindleBookJson);
            }

            log_d("Parsed %zu Kindle books from sync response", kindleBooks.size());
        } else {
            kindleLastSynced = "";
            log_d("No Kindle books returned from sync response");
        }

        if (results.containsKey("timezone")) {
            timezone = results["timezone"].as<std::string>();
        }

        if (results.containsKey("email")) {
            email = results["email"].as<std::string>();
        }

        // Parse book_questions if present
        hasBookQuestions = false;
        if (results.containsKey("book_questions")) {
            hasBookQuestions = true;
            bookQuestions.fromJson(results["book_questions"].as<JsonObject>());
            log_d("Parsed book questions: version=%d, questions=%zu, contexts=%zu",
                  bookQuestions.version, bookQuestions.questions.size(),
                  bookQuestions.contexts.size());
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
