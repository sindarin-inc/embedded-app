#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/ReadableDictionary.hpp"
#include "Models/UserReadablePartialChapterSummary.hpp"

using ReadableDictionaryVector = SpiramVector<ReadableDictionary>;

class ReadableTableOfContentsChapter : public JsonSerializable {
public:
    ReadableTableOfContentsChapter() = default;
    ~ReadableTableOfContentsChapter() override = default;

    SpiramString title;
    std::string href;
    uint32_t txtOffset;
    uint32_t endTxtOffset;
    SpiramString inSubsectionHref;
    SpiramString summary;
    std::shared_ptr<UserReadablePartialChapterSummary> partialChapterSummary = nullptr;
    ReadableDictionaryVector dictionary;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("href") || !j.containsKey("txt_offset") || !j.containsKey("title")) {
            return false;
        }
        title = j["title"].as<std::string>();
        href = j["href"].as<std::string>();
        if (j.containsKey("in_subsection_href")) {
            inSubsectionHref = j["in_subsection_href"].as<std::string>();
        }
        txtOffset = j["txt_offset"].as<uint32_t>();
        if (j.containsKey("end_txt_offset") && !j["end_txt_offset"].isNull()) {
            endTxtOffset = j["end_txt_offset"].as<uint32_t>();
        }
        if (j.containsKey("summary") && !j["summary"].isNull()) {
            summary = j["summary"].as<std::string>();
        }
        if (j.containsKey("partial_chapter_summary") && !j["partial_chapter_summary"].isNull()) {
            partialChapterSummary = SpiRamPtr<UserReadablePartialChapterSummary>();
            partialChapterSummary->fromJson(j["partial_chapter_summary"]);
        }

        dictionary.clear();
        if (j.containsKey("dictionary")) {
            for (const auto &definition : j["dictionary"].as<JsonArray>()) {
                dictionary.emplace_back();
                dictionary.back().fromJson(definition);
            }
        }
        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableTableOfContentsChapter &src, JsonVariant dst) -> bool;
