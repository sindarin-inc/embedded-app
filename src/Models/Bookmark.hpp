#pragma once

#include <string>

#include "Models/JsonSerializable.hpp"

class Bookmark : public JsonSerializable {
public:
    Bookmark() = default;
    ~Bookmark() override = default;

    std::string uuid;
    int starting_position;
    int ending_position;
    std::string readable;
    std::string userReadable;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("name") || !j.containsKey("description")) {
            return false;
        }

        uuid = j["uuid"].as<std::string>();
        starting_position = j["starting_position"].as<int>();
        ending_position = j["ending_position"].as<int>();
        readable = j["readable"].as<std::string>();
        userReadable = j["userReadable"].as<std::string>();

        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Bookmark &src, JsonVariant dst) -> bool;
