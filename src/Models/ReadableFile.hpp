#pragma once

#include "Misc/SpiramAllocator.hpp"
#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"

class ReadableFile : public JsonSerializable {
public:
    ReadableFile() = default;
    ~ReadableFile() override = default;

    std::string uuid;
    SpiramString name;
    int size;
    SpiramString filetype;
    SpiramString uploadedAt;
    SpiramString shaHash;
    //! URL to download the file
    SpiramString file;
    SpiramString fileProxied;
    SpiramString readableType;

    auto fromJson(const JsonObject &j) -> bool override {

        readableType = j["readable_type"].as<std::string>();
        if (readableType == "pocket_article" || readableType == "email_article") {
            if (!j.containsKey("file") || !j.containsKey("size")) {
                log_w("Failed to parse article ReadableFile %s", DumpJson(j).c_str());
                return false;
            }
        } else {
            if (!j.containsKey("uuid") || !j.containsKey("name") || !j.containsKey("size") ||
                !j.containsKey("filetype") || !j.containsKey("uploaded_at") ||
                !j.containsKey("sha_hash")) {
                log_w("Failed to parse ReadableFile %s", DumpJson(j).c_str());
                return false;
            }
        }

        uuid = j["uuid"].as<std::string>();
        name = j["name"].as<std::string>();
        size = j["size"];
        filetype = j["filetype"].as<std::string>();
        uploadedAt = j["uploaded_at"].as<std::string>();
        shaHash = j["sha_hash"].as<std::string>();
        file = j["file"].as<std::string>();

        if (j.containsKey("file_proxied")) {
            fileProxied = j["file_proxied"].as<std::string>();
        } else {
            fileProxied = file;
        }
        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableFile &src, JsonVariant dst) -> bool;
