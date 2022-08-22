#pragma once

#include <string>

#include "Models/JsonSerializable.hpp"

class Author : public JsonSerializable {
public:
    int id;
    std::string name;
    std::string description;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("id") || !j.containsKey("name") || !j.containsKey("description")) {
            return false;
        }
        id = j["id"];
        name = j["name"].as<std::string>();
        description = j["description"].as<std::string>();
        return true;
    }
};

bool convertToJson(const Author &src, JsonVariant dst);
