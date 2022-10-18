#include <string>

#include "Models/Firmware.hpp"
#include "Models/JsonSerializable.hpp"

class CheckSyncResponse : JsonSerializable {
public:
    CheckSyncResponse() = default;
    ~CheckSyncResponse() override = default;

    bool synced;
    std::string message;
    Firmware firmware;

    auto fromJson(const JsonObject &j) -> bool override {
        synced = j["synced"].as<bool>();
        message = j["message"].as<std::string>();
        firmware.fromJson(j["firmware"]);
        return true;
    }
};
