#pragma once

#include <string>

#include "Models/JsonSerializable.hpp"

class Author : public JsonSerializable {
public:
    Author() = default;
    ~Author() override = default;

    std::string id;
    std::string name;
    std::string description;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("id") || !j.containsKey("name") || !j.containsKey("description")) {
            return false;
        }
        id = j["id"].as<std::string>();
        name = j["name"].as<std::string>();
        description = j["description"].as<std::string>();
        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Author &src, JsonVariant dst) -> bool;
