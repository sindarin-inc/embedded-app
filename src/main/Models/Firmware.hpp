#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class Firmware : public JsonSerializable {
public:
    Firmware() = default;
    ~Firmware() override = default;

    std::string version;
    std::string url;
    std::string md5;
    SpiramString releaseNotes;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("version") || !j.containsKey("url") ||
            (!j.containsKey("md5") && !j.containsKey("md5_hash"))) {
            return false;
        }
        version = j["version"].as<std::string>();
        url = j["url"].as<std::string>();
        md5 = j["md5"].as<std::string>();
        // /sync/all uses 'md5' and /firmware uses 'md5_hash'
        md5 = j["md5"] | j["md5_hash"].as<std::string>();
        releaseNotes = j["release_notes"].as<std::string>();
        return true;
    }
};
