#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class ReadableDictionary : public JsonSerializable {
public:
    ReadableDictionary() = default;
    ~ReadableDictionary() override = default;

    SpiramString word;
    SpiramString definition;
    std::vector<unsigned long> locations;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("w") || !j.containsKey("d") || !j.containsKey("l")) {
            return false;
        }
        word = j["w"].as<std::string>();
        definition = j["d"].as<std::string>();
        for (const auto &location : j["l"].as<JsonArray>()) {
            locations.push_back(location.as<uint32_t>());
        }

        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableDictionary &src, JsonVariant dst) -> bool;
