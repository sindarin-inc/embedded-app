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
        -> SpiramVector<UserReadablePlacemarkHistory> {

        auto placemarkHistoryPath = readable->placemarkHistoryFilePath();
        auto placemarkHistoryFile = fileSystem.open(placemarkHistoryPath, FILE_READ, false);
        if (!placemarkHistoryFile) {
            log_w("Failed to open placemark history file for reading");
            return {};
        }
        defer { placemarkHistoryFile.close(); };

        auto placemarkHistoryDoc = std::make_unique<SpiRamJsonDocument>(
            std::max(static_cast<size_t>(50000), 2 * placemarkHistoryFile.size()));
        auto error = deserializeJson(*placemarkHistoryDoc, placemarkHistoryFile);
        if (error) {
            log_e("Failed to parse placemark history JSON: %s", error.c_str());
            return {};
        }

        auto placemarkHistory = SpiramVector<UserReadablePlacemarkHistory>();
        auto placemarkHistoryJson = (*placemarkHistoryDoc)["placemark_history"].as<JsonArray>();
        for (const auto &placemark : placemarkHistoryJson) {
            placemarkHistory.emplace_back();
            placemarkHistory.back().fromJson(placemark);
        }
        log_d("Loaded %zu placemark histories from %s", placemarkHistory.size(),
              placemarkHistoryPath.c_str());
        placemarkHistoryFile.close();
        return placemarkHistory;
    }

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
        return ePubPlace == other.ePubPlace && placemark == other.placemark &&
               dateCreated == other.dateCreated;
    }

    SpiramString deviceSn;
    std::shared_ptr<EPubPlace> ePubPlace;
    uint8_t placemark{};
    SpiramString dateCreated;
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadablePlacemarkHistory &src, JsonVariant dst) -> bool;
