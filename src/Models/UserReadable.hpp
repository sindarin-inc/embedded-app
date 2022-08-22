#pragma once

#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Readable.hpp"

class UserReadable : JsonSerializable {
public:
    UserReadable() = default;

    int id;
    Readable readable;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("id") || !j.containsKey("readable")) {
            log_w("Failed to parse UserReadable with JSON %s", DumpJson(j).c_str());
            return false;
        }
        id = j["id"];
        return readable.fromJson(j["readable"]);
    }

    operator std::string() const { return std::to_string(id); }
};

bool convertToJson(const UserReadable &src, JsonVariant dst);
