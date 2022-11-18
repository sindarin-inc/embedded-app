#pragma once

#include <string>

#include "Models/JsonSerializable.hpp"

class Placemark : public JsonSerializable {
public:
    Placemark() = default;
    ~Placemark() override = default;

    std::string uuid;
    std::string readableUuid;
    uint32_t placemark;
    std::string dateCreated;
    std::string dateModified;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("readable_uuid") ||
            !j.containsKey("placemark")) {
            return false;
        }
        uuid = j["uuid"].as<std::string>();
        readableUuid = j["readable_uuid"].as<std::string>();
        placemark = j["placemark"].as<uint32_t>();
        dateCreated = j["date_created"].as<std::string>();
        dateModified = j["date_modified"].as<std::string>();
        return true;
    }
};
