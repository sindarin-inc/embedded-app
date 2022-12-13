#include <string>

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
    std::vector<UserReadable> userReadables;
    std::vector<UserReadable> userArticles;
    std::string readablesSince;
    std::string articlesSince;

    auto fromJson(const JsonObject &j) -> bool override {
        synced = j["synced"].as<bool>();
        message = j["message"].as<std::string>();
        readablesSince = j["readables_since"].as<std::string>();
        articlesSince = j["articles_since"].as<std::string>();
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
