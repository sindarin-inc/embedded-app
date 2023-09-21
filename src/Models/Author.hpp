#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class Author : public JsonSerializable {
public:
    Author() = default;
    ~Author() override = default;

    SpiramString uuid;
    SpiramString name;
    SpiramString description;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("name") || !j.containsKey("description")) {
            return false;
        }
        uuid = j["uuid"].as<std::string>();
        name = j["name"].as<std::string>();
        description = j["description"].as<std::string>();
        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Author &src, JsonVariant dst) -> bool;
