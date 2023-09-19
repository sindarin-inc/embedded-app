#pragma once

#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Readable.hpp"

class UserReadable : JsonSerializable {
public:
    UserReadable() = default;
    ~UserReadable() override = default;

    SpiramString uuid;
    uint32_t placemark;
    bool isArchived;
    std::shared_ptr<Readable> readable = nullptr;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("readable")) {
            log_w("Failed to parse UserReadable with JSON %s", DumpJson(j).c_str());
            return false;
        }
        uuid = j["uuid"].as<std::string>();
        placemark = j["placemark"].as<uint32_t>();
        isArchived = j["is_archived"].as<bool>();
        readable = SpiRamPtr<Readable>();
        return readable->fromJson(j["readable"]);
    }

    operator std::string() const { return std::string(uuid); }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadable &src, JsonVariant dst) -> bool;
