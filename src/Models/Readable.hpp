#pragma once

#include <cstdint>
#include <deque>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "Misc/SpiramAllocator.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/TelemetryVariant.hpp"
#include "Models/Author.hpp"
#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Place.hpp"
#include "Models/ReadableArchive.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Models/ReadableDictionary.hpp"
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

    //! Constructor for local-only in-firmware readables
    Readable(const std::string &uuid, const std::string &title, const unsigned char *localBytes,
             size_t localBytesSize)
        : uuid(uuid), shortenedUuid(uuid), title(title), localOnly(true), localBytes(localBytes),
          localBytesSize(localBytesSize) {
        shortenedUuid.resize(TRUNCATE_READABLE_UUID_FOR_PLACEMARK_BYTES);
    }

    ~Readable() override = default;

    SpiramString uuid;
    SpiramString shortenedUuid;
    SpiramString title;
    // TODO: Remove
    SpiramString content;
    SpiramString url;
    SpiramString dateCreated;
    SpiramString dateModified;
    time_t dateModifiedTime{};
    SpiramString datePublished;
    SpiramString processedBy;
    ReadableAuthorVector authors;
    ReadableFileVector files;
    ReadableBookmarksVector bookmarks;

    // Local state
    std::shared_ptr<Place> place, placePrev;

    SpiramString readableType;
    // TODO: source
    // TODO: categories

    //! Set to true if this is a readable side-loaded onto the device (that doesn't come from the
    //! server)
    bool localOnly = false;

    // If the readable came from the app, this will be set to true
    bool appProcessed = false;

    // For embedded-in-firmware epubs, this is the pointer to the epub's info
    const unsigned char *localBytes = nullptr;
    size_t localBytesSize = 0;

    [[nodiscard]] auto file(const std::string &path = "") const -> const ReadableFile & {
        for (const auto &file : files) {
            if (file.filetype == "epub" && (path.empty() || HasSuffix(path, ".epub"))) {
                return file;
            }
        }

        for (const auto &file : files) {
            if (file.filetype == "utf8_txt" &&
                (path.empty() || HasSuffix(path, ".txt") || HasSuffix(path, ".txt.gz"))) {
                return file;
            }
        }

        for (const auto &file : files) {
            if (file.filetype == "txt" &&
                (path.empty() || HasSuffix(path, ".txt") || HasSuffix(path, ".txt.gz"))) {
                return file;
            }
        }

        return files[0];
    }

    // TODO: Move file path operations to LocalStore
    [[nodiscard]] auto bookmarkFilePath() const -> std::string {
        return StringFormat("/bookmarks/%s-bookmark.txt", uuid.c_str());
    }
    [[nodiscard]] auto tocFilePath() const -> std::string {
        return StringFormat("/toc/%s-toc.json", uuid.c_str());
    }
    [[nodiscard]] auto partialSummaryFilePath() const -> std::string {
        return StringFormat("/summary/%s-summary.json", uuid.c_str());
    }
    [[nodiscard]] auto placemarkHistoryFilePath() const -> std::string {
        return StringFormat("/placemark_history/%s-placemark-history.json", uuid.c_str());
    }
    [[nodiscard]] auto remoteUrl() const -> SpiramString { return file().fileProxied; }
    [[nodiscard]] auto isArticle() const -> bool {
        return readableType == "pocket_article" || readableType == "email_article";
    }

    [[nodiscard]] auto telemetryPlacemarkMetrics() const
        -> std::unordered_map<std::string, TelemetryVariant>;

    [[nodiscard]] auto authorString() const -> SpiramString {
        if (authors.empty()) {
            log_w("Blank author for %s", title.c_str());
            return "";
        }
        return authors[0].name;
    }

    auto parseDate(const std::string &dateStr) -> time_t {
        struct tm tm = {};
        std::istringstream ss(dateStr.substr(0, 19)); // Only parse the date and time part
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

        if (ss.fail()) {
            log_e("Failed to parse date string from %s", dateStr.c_str());
        }

        // Disable automatic DST adjustment
        tm.tm_isdst = -1;

        return mktime(&tm);
    };

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
        dateModifiedTime = parseDate(std::string(dateModified));
        datePublished = j["date_published"].as<std::string>();
        processedBy = j["processed_by"].as<std::string>();

        if (processedBy.substr(0, 11) == "desktop_app") {
            appProcessed = true;
        }

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
