#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class Author : public JsonSerializable {
public:
    Author() = default;
    Author(const std::string &uuid, const std::string &name, const std::string &description)
        : uuid(uuid), name(name), description(description) {}
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
