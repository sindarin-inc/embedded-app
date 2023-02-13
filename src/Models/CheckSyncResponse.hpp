#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/Firmware.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/UserReadable.hpp"

class CheckSyncResponse : JsonSerializable {
public:
    CheckSyncResponse() = default;
    ~CheckSyncResponse() override = default;

    bool synced;
    std::string message;
    Firmware firmware;
    std::vector<UserReadable, SpiramAllocator<UserReadable>> userReadables;
    std::vector<UserReadable, SpiramAllocator<UserReadable>> userArticles;
    std::string readablesSince;
    std::string articlesSince;

    auto fromJson(const JsonObject &j) -> bool override {
        synced = j["synced"].as<bool>();
        message = j["message"].as<std::string>();
        if (j.containsKey("readables_since")) {
            readablesSince = j["readables_since"].as<std::string>();
        } else {
            readablesSince = "";
        }
        if (j.containsKey("articles_since")) {
            articlesSince = j["articles_since"].as<std::string>();
        } else {
            articlesSince = "";
        }
        firmware.fromJson(j["firmware"]);
        userReadables.clear();
        for (const auto &userReadable : j["readables"].as<JsonArray>()) {
            userReadables.emplace_back();
            userReadables.back().fromJson(userReadable);
        }
        userArticles.clear();
        if (j.containsKey("articles")) {
            for (const auto &userArticle : j["articles"].as<JsonArray>()) {
                userArticles.emplace_back();
                userArticles.back().fromJson(userArticle);
            }
        }
        return true;
    }
};
