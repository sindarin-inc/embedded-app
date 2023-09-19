#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class ReadableArchive : public JsonSerializable {
public:
    ReadableArchive() = default;
    ~ReadableArchive() override = default;

    SpiramString uuid;
    bool isArchived;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("is_archived")) {
            return false;
        }
        uuid = j["uuid"].as<std::string>();
        isArchived = j["is_archived"].as<bool>();
        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableArchive &src, JsonVariant dst) -> bool;
