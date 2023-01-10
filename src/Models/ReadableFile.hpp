#pragma once

#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"

class ReadableFile : public JsonSerializable {
public:
    ReadableFile() = default;
    ~ReadableFile() override = default;

    std::string uuid;
    std::string name;
    int size;
    std::string filetype;
    std::string uploadedAt;
    std::string shaHash;
    //! URL to download the file
    std::string file;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("name") || !j.containsKey("size") ||
            !j.containsKey("filetype") || !j.containsKey("uploaded_at") ||
            !j.containsKey("sha_hash")) {
            log_w("Failed to parse ReadableFile with JSON %s", DumpJson(j).c_str());
            return false;
        }
        uuid = j["uuid"].as<std::string>();
        name = j["name"].as<std::string>();
        size = j["size"];
        filetype = j["filetype"].as<std::string>();
        uploadedAt = j["uploaded_at"].as<std::string>();
        shaHash = j["sha_hash"].as<std::string>();
        file = j["file"].as<std::string>();
        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableFile &src, JsonVariant dst) -> bool;
