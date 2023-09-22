#pragma once

#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Place.hpp"
#include "Models/Readable.hpp"

class UserReadable : JsonSerializable {
public:
    UserReadable() = default;
    ~UserReadable() override = default;

    SpiramString uuid;
    std::shared_ptr<Place> txtPlace;
    std::shared_ptr<Place> ePubPlace;

    bool isArchived;
    std::shared_ptr<Readable> readable = nullptr;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("readable")) {
            log_w("Failed to parse UserReadable with JSON %s", DumpJson(j).c_str());
            return false;
        }

        uuid = j["uuid"].as<std::string>();

        if (j.containsKey("placemark")) {
            txtPlace = Place::factory(j["placemark"].as<uint32_t>());
        } else {
            txtPlace = std::make_shared<NonePlace>();
        }
        if (j.containsKey("chapter") && j.containsKey("chapter_offset")) {
            ePubPlace =
                Place::factory(j["chapter_offset"].as<uint32_t>(), j["chapter"].as<std::string>());
        } else {
            ePubPlace = std::make_shared<NonePlace>();
        }
        if (txtPlace->isFor(DocType::NONE) && ePubPlace->isFor(DocType::NONE)) {
            log_w("Failed to parse UserReadable with JSON: No Txt or EPub location detected.");
            return false;
        }

        isArchived = j["is_archived"].as<bool>();
        readable = SpiRamPtr<Readable>();

        return readable->fromJson(j["readable"]);
    }

    operator std::string() const { return std::string(uuid); }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadable &src, JsonVariant dst) -> bool;
