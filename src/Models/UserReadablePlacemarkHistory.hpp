#pragma once

#include <string>

#include "Misc/Defer.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Place.hpp"
#include "Storage/FileSystem.hpp"

class UserReadablePlacemarkHistory : public JsonSerializable {
public:
    UserReadablePlacemarkHistory() = default;
    ~UserReadablePlacemarkHistory() override = default;

    static auto loadForReadable(FileSystem &fileSystem, const std::shared_ptr<Readable> &readable)
        -> std::shared_ptr<UserReadablePlacemarkHistory> {
        auto placemarkHistoryPath = readable->placemarkHistoryFilePath();
        auto file = fileSystem.open(placemarkHistoryPath, FILE_READ, false);
        // Check if placemark history already exists
        if (!file) {
            return nullptr;
        }
        defer { file.close(); };

        auto doc = std::make_unique<SpiRamJsonDocument>(file.size() * 2);
        auto error = deserializeJson(*doc, file);
        if (error.code() != DeserializationError::Ok) {
            log_e("Failed to deserialize placemark history file: %s", error.c_str());
            return nullptr;
        }

        auto placemarkHistory = std::make_shared<UserReadablePlacemarkHistory>();
        if (!placemarkHistory->fromJson(doc->as<JsonObject>())) {
            log_e("Failed to parse placemark history file: %s", placemarkHistoryPath.c_str());
            return nullptr;
        }

        return placemarkHistory;
    }

    SpiramString deviceSn;
    std::shared_ptr<EPubPlace> ePubPlace;
    uint8_t placemark{};
    SpiramString dateCreated;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("device_sn") || !j.containsKey("placemark") ||
            !j.containsKey("date_created") || !j.containsKey("chapter") ||
            !j.containsKey("chapter_offset")) {
            return false;
        }
        deviceSn = j["device_sn"].as<std::string>();
        ePubPlace = std::dynamic_pointer_cast<EPubPlace>(
            Place::factory(j["chapter_offset"].as<uint32_t>(), j["chapter"].as<std::string>()));
        placemark = j["placemark"].as<uint8_t>();
        dateCreated = j["date_created"].as<std::string>();

        if (ePubPlace->isZero()) {
            return false;
        }

        return true;
    }

    auto operator==(const UserReadablePlacemarkHistory &other) const -> bool {
        return ePubPlace == other.ePubPlace || placemark == other.placemark ||
               dateCreated == other.dateCreated;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadablePlacemarkHistory &src, JsonVariant dst) -> bool;
