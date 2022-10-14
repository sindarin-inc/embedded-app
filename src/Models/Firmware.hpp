#pragma once

#include <string>

#include "Models/JsonSerializable.hpp"

class Firmware : public JsonSerializable {
public:
    Firmware() = default;
    ~Firmware() override = default;

    std::string version;
    std::string url;
    std::string md5;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("version") || !j.containsKey("url") || !j.containsKey("md5")) {
            return false;
        }
        version = j["version"].as<std::string>();
        url = j["url"].as<std::string>();
        md5 = j["md5"].as<std::string>();
        return true;
    }
};
