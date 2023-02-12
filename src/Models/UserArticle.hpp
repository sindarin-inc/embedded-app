#pragma once

#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Readable.hpp"

class UserArticle : JsonSerializable {
public:
    UserArticle() = default;
    ~UserArticle() override = default;

    std::string uuid;
    uint32_t placemark;
    bool isArchived;
    Readable readable;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("readable")) {
            log_w("Failed to parse UserArticle with JSON %s", DumpJson(j).c_str());
            return false;
        }
        uuid = j["uuid"].as<std::string>();
        placemark = j["placemark"].as<uint32_t>();
        isArchived = j["is_archived"].as<bool>();
        return readable.fromJson(j["readable"]);
    }

    operator std::string() const { return uuid; }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserArticle &src, JsonVariant dst) -> bool;
