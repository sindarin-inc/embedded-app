#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Place.hpp"

class ReadablePlacemark : public JsonSerializable {
public:
    ReadablePlacemark() = default;
    ~ReadablePlacemark() override = default;

    std::string uuid;
    std::string position;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("position")) {
            return false;
        }
        uuid = j["uuid"].as<std::string>();
        position = j["position"].as<std::string>();
        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadablePlacemark &src, JsonVariant dst) -> bool;
