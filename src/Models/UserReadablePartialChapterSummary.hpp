#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Place.hpp"

class UserReadablePartialChapterSummary : public JsonSerializable {
public:
    UserReadablePartialChapterSummary() = default;
    ~UserReadablePartialChapterSummary() override = default;

    SpiramString text;
    std::shared_ptr<EPubPlace> ePubPlace;
    uint8_t readPct;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("text") || !j.containsKey("chapter") ||
            !j.containsKey("chapter_offset") || !j.containsKey("read_pct")) {
            return false;
        }
        text = j["text"].as<std::string>();
        ePubPlace = std::dynamic_pointer_cast<EPubPlace>(
            Place::factory(j["chapter_offset"].as<uint32_t>(), j["chapter"].as<std::string>()));
        readPct = j["read_pct"].as<uint8_t>();

        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadablePartialChapterSummary &src, JsonVariant dst) -> bool;
