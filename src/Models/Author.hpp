#pragma once

#include <string>

#include "Models/JsonSerializable.hpp"

// Used to create a many-to-many relationship between Author and Readable
struct ReadableAuthor {
    // To make it easy to do idempotent operations the ID is of format [readable_uuid]-[author_uuid]
    std::string id;
    std::string readableUuid;
    std::string authorUuid;
};

class Author : public JsonSerializable {
public:
    Author() = default;
    ~Author() override = default;

    std::string uuid;
    std::string name;
    std::string description;

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
