#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class ReadableTableOfContentsChapter : public JsonSerializable {
public:
    ReadableTableOfContentsChapter() = default;
    ~ReadableTableOfContentsChapter() override = default;

    std::string title;
    std::string href;
    uint32_t txtOffset;
    std::string inSubsectionHref;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("href") || !j.containsKey("txt_offset") || !j.containsKey("title")) {
            return false;
        }
        title = j["title"].as<std::string>();
        href = j["href"].as<std::string>();
        if (j.containsKey("in_subsection_href")) {
            inSubsectionHref = j["in_subsection_href"].as<std::string>();
            if (!inSubsectionHref.empty()) {
                title = "  " + title;
            }
        }
        txtOffset = j["txt_offset"].as<uint32_t>();

        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableTableOfContentsChapter &src, JsonVariant dst) -> bool;
