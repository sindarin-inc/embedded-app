#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Misc/SpiramAllocator.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Author.hpp"
#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/ReadableArchive.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Models/ReadableFile.hpp"
#include "Models/ReadablePlacemark.hpp"
#include "Models/ReadableTableOfContentsChapter.hpp"

// This is #define because there might be different types passed in
#define SOL_PAGE_NUMBER(x) ((x) / 1000 + 1)

#define TRUNCATE_READABLE_UUID_FOR_PLACEMARK_BYTES 8

using ReadableAuthorVector = SpiramVector<Author>;
using ReadableFileVector = SpiramVector<ReadableFile>;
using ReadableArchivesVector = SpiramVector<ReadableArchive>;
using ReadablePlacemarksVector = SpiramVector<ReadablePlacemark>;
using ReadableBookmarksVector = SpiramVector<ReadableBookmark>;
using ReadableTableOfContentsChapterVector = SpiramVector<ReadableTableOfContentsChapter>;

class Readable : JsonSerializable {
public:
    Readable() = default;
    ~Readable() override = default;

    std::string uuid;
    SpiramString shortenedUuid;
    SpiramString title;
    // TODO: Remove
    SpiramString content;
    SpiramString url;
    SpiramString dateCreated;
    SpiramString dateModified;
    SpiramString datePublished;
    ReadableAuthorVector authors;
    ReadableFileVector files;
    ReadableBookmarksVector bookmarks;
    ReadableTableOfContentsChapterVector toc;

    SpiramString readableType;
    // TODO: source
    // TODO: categories

    //! Set to true if this is a readable side-loaded onto the device (that doesn't come from the
    //! server)
    bool localOnly = false;
    // Local state
    uint32_t place{0}, placePrev{0};

    [[nodiscard]] auto file() const -> const ReadableFile & {
        for (const auto &file : files) {
            if (file.filetype == "utf8_txt") {
                return file;
            }
        }

        for (const auto &file : files) {
            if (file.filetype == "txt") {
                return file;
            }
        }

        return files[0];
    }

    // TODO: Move file path operations to LocalStore
    [[nodiscard]] auto bookmarkFilePath() const -> std::string {
        return StringFormat("/bookmarks/%s-bookmark.txt", uuid.c_str());
    }
    [[nodiscard]] auto remoteUrl() const -> SpiramString { return file().fileProxied; }
    [[nodiscard]] auto isArticle() const -> bool {
        return readableType == "pocket_article" || readableType == "email_article";
    }

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("title") || !j.containsKey("date_created") ||
            !j.containsKey("date_modified") || !j.containsKey("authors")) {

            log_w("Failed to parse Readable %s", DumpJson(j).c_str());
            return false;
        }
        // log_d("Parsing Readable %s", DumpJson(j).c_str());
        uuid = j["uuid"].as<std::string>();
        shortenedUuid = j["uuid"].as<std::string>();
        shortenedUuid.resize(TRUNCATE_READABLE_UUID_FOR_PLACEMARK_BYTES);
        title = j["title"].as<std::string>();
        dateCreated = j["date_created"].as<std::string>();
        dateModified = j["date_modified"].as<std::string>();
        datePublished = j["date_published"].as<std::string>();
        localOnly = j["local_only"].as<bool>();
        authors.clear();
        for (const auto &author : j["authors"].as<JsonArray>()) {
            authors.emplace_back();
            authors.back().fromJson(author);
        }
        bookmarks.clear();
        for (const auto &bookmark : j["bookmarks"].as<JsonArray>()) {
            bookmarks.emplace_back();
            bookmarks.back().fromJson(bookmark);
        }
        toc.clear();
        for (const auto &chapter : j["toc"].as<JsonArray>()) {
            toc.emplace_back();
            toc.back().fromJson(chapter);
        }

        // Temporary migration for stored pocket articles that don't have a readable_type
        // This can probably be removed in a few weeks ~mid-march 2023
        if (j["readable_type"].isNull()) {
            if (j["files"].as<JsonArray>().size() == 0) {
                readableType = "pocket_article";
            } else {
                readableType = "book";
            }
            log_d("Migrating readable_type for %s to %s", uuid.c_str(), readableType.c_str());
        } else {
            readableType = j["readable_type"].as<std::string>();
        }

        files.clear();
        for (const auto &file : j["files"].as<JsonArray>()) {
            // files handled differently for articles, record if it is an article
            file["readable_type"] = readableType;
            files.emplace_back();
            files.back().fromJson(file);
        }
        // TODO: Instead of reading content, write it to disk immediately
        content = j["content"].as<std::string>();

        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Readable &src, JsonVariant dst) -> bool;
