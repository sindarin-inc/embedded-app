#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class ReadablePlacemark : public JsonSerializable {
public:
    ReadablePlacemark() = default;
    ~ReadablePlacemark() override = default;

    std::string uuid;
    uint32_t position;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("position")) {
            return false;
        }
        uuid = j["uuid"].as<std::string>();
        position = j["position"].as<uint32_t>();
        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadablePlacemark &src, JsonVariant dst) -> bool;
