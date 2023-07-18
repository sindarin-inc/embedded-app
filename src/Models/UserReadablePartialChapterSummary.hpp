#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class UserReadablePartialChapterSummary : public JsonSerializable {
public:
    UserReadablePartialChapterSummary() = default;
    ~UserReadablePartialChapterSummary() override = default;

    std::string text;
    uint32_t placemark;
    uint8_t readPct;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("text") || !j.containsKey("placemark") || !j.containsKey("read_pct")) {
            return false;
        }
        text = j["text"].as<std::string>();
        placemark = j["placemark"].as<uint32_t>();
        readPct = j["read_pct"].as<uint8_t>();

        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadablePartialChapterSummary &src, JsonVariant dst) -> bool;
