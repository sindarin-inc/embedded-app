#pragma once

#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Readable.hpp"

class UserReadable : JsonSerializable {
public:
    UserReadable() = default;
    ~UserReadable() override = default;

    std::string id;
    Readable readable;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("id") || !j.containsKey("readable")) {
            log_w("Failed to parse UserReadable with JSON %s", DumpJson(j).c_str());
            return false;
        }
        id = j["id"].as<std::string>();
        return readable.fromJson(j["readable"]);
    }

    operator std::string() const { return id; }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadable &src, JsonVariant dst) -> bool;
