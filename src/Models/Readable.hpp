#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <string>
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
    ~Readable() override = default;

    SpiramString uuid;
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
    // hasSummary denotes if any of the chapters have a summary, used for determining if we should
    // show the summary menu item
    bool hasSummary = false;
    bool hasDictionary = false;

    // Local state
    std::shared_ptr<Place> place, placePrev;

    SpiramString readableType;
    // TODO: source
    // TODO: categories

    //! Set to true if this is a readable side-loaded onto the device (that doesn't come from the
    //! server)
    bool localOnly = false;

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
    [[nodiscard]] auto remoteUrl() const -> SpiramString { return file().fileProxied; }
    [[nodiscard]] auto isArticle() const -> bool {
        return readableType == "pocket_article" || readableType == "email_article";
    }
    auto findChapter(unsigned long placeInReadable) -> ReadableTableOfContentsChapter * {
        for (auto &chapter : toc) {
            if (placeInReadable >= chapter.txtOffset && placeInReadable < chapter.endTxtOffset) {
                return &chapter;
            }
        }
        return nullptr;
    }

    auto findEPubHRefChapterIdx(const std::string &href) -> int16_t {
        int16_t idx = 0;
        for (auto &chapter : toc) {
            if (chapter.href == href) {
                return idx;
            }
            idx++;
        }
        return -1;
    }

    [[nodiscard]] auto telemetryPlacemarkMetrics() const -> std::map<std::string, TelemetryVariant>;

    auto summary(const std::shared_ptr<Place> &placeInReadable,
                 const std::shared_ptr<TxtPlace> &placeInSummary, bool includeUnread,
                 int currentChapterIdx) -> SpiramString {
        SpiramString summary;

        for (const auto &chapter : toc) {
            auto sameChapter =
                (placeInReadable->isFor(DocType::EPUB) &&
                 chapter.href ==
                     std::dynamic_pointer_cast<EPubPlace>(placeInReadable)->getHRef()) ||
                (placeInReadable->isFor(DocType::TXT) &&
                 chapter.txtOffset < placeInReadable->getOffset() &&
                 placeInReadable->getOffset() < chapter.endTxtOffset);
            auto futureChapter = (placeInReadable->isFor(DocType::EPUB) &&
                                  findEPubHRefChapterIdx(chapter.href) > currentChapterIdx) ||
                                 (placeInReadable->isFor(DocType::TXT) &&
                                  placeInReadable->getOffset() < chapter.txtOffset);
            auto pastOrSameChapter = (placeInReadable->isFor(DocType::EPUB) &&
                                      findEPubHRefChapterIdx(chapter.href) <= currentChapterIdx) ||
                                     (placeInReadable->isFor(DocType::TXT) &&
                                      placeInReadable->getOffset() >= chapter.txtOffset);

            if (pastOrSameChapter) {
                placeInSummary->setOffset(summary.length());
            }

            if (futureChapter && !includeUnread) {
                break;
            }

            if (sameChapter && !includeUnread && chapter.partialChapterSummary != nullptr &&
                !chapter.partialChapterSummary->text.empty()) {
                summary += chapter.title;
                summary += StringFormat(" (%d%% read)\n\n", chapter.partialChapterSummary->readPct);
                summary += chapter.partialChapterSummary->text;
                summary += "\n\n";
            } else {
                summary += chapter.title;
                summary += "\n\n";
                if (!chapter.summary.empty()) {
                    summary += chapter.summary;
                    summary += "\n\n";
                }
            }
        }

        return summary;
    }

    auto definitions(const std::shared_ptr<Place> &placeInReadable,
                     const std::shared_ptr<Place> &placeInDictionaryOut) -> SpiramString {
        // This code generates the dictionaryString for an entire chapter, making sure to not
        // add duplicate words within the last 5 words (but duplicates are allowed assuming they
        // are > 5 words ago). It returns the dictionaryString as well as the position of the
        // word closest to the current page using the placeInDictionaryOut out-value.

        // NOTE(john): Punting for now since this is going to get included in the epub file
        return "";
        /*
        SpiramString dictionaryString;

        std::deque<SpiramString> lastFiveWords;

        log_d("Looking for definitions for place %s in %zu chapters",
        placeInReadable.str().c_str(), toc.size());

        auto chapter = findChapter(placeInReadable);
        if (chapter == nullptr) {
            log_e("Failed to find chapter for place %lu", placeInReadable);
            return "No definitions found in this chapter.";
        }

        std::map<unsigned long, ReadableDictionary *> locationDictMap;

        for (ReadableDictionary &dict : chapter->dictionary) {
            for (const auto &location : dict.locations) {
                locationDictMap[location] = &dict;
            }
        }

        log_d("Found %zu definitions", locationDictMap.size());

        for (const auto &entry : locationDictMap) {
            const auto &dict = *(entry.second);

            if (std::find(lastFiveWords.begin(), lastFiveWords.end(), dict.word) !=
                lastFiveWords.end()) {
                continue;
            }

            if (placeInReadable >= (chapter->txtOffset + entry.first)) {
                *placeInDictionary = dictionaryString.length();
            }

            dictionaryString += dict.word + ": " + dict.definition + "\n\n";

            lastFiveWords.push_back(dict.word);
            if (lastFiveWords.size() > 5) {
                lastFiveWords.pop_front();
            }
        }

        if (locationDictMap.empty()) {
            log_w("No definitions found for %s", title.c_str());
            return "No definitions found in this chapter.";
        }

        return dictionaryString;
        */
    }

    [[nodiscard]] auto authorString() const -> SpiramString {
        if (authors.size() == 0) {
            log_w("Blank author for %s", title.c_str());
            return "";
        }
        return authors[0].name;
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
            // Check if any chapter has a summary, if so, set hasSummary
            if (!hasSummary && chapter["summary"].as<std::string>().size() > 0) {
                hasSummary = true;
            }
            if (!hasDictionary && chapter["dictionary"].as<JsonArray>().size() > 0) {
                hasDictionary = true;
            }
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
