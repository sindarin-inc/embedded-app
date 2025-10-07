#pragma once

#include <Spiram/String.hpp>
#include <string>

#include "Models/JsonSerializable.hpp"
#include "Models/Place.hpp"

class ReadablePlacemark : public JsonSerializable {
public:
    ReadablePlacemark() = default;
    ~ReadablePlacemark() override = default;

    SpiramString uuid;
    std::shared_ptr<Place> place;

    auto fromJson(const JsonObject &j) -> bool override {
        uuid.clear();

        if (j.containsKey("uuid")) {
            uuid = j["uuid"].as<std::string>();
            if (j.containsKey("chapter") && j.containsKey("chapter_offset")) {
                place = Place::factory(j["chapter_offset"].as<std::uint32_t>(),
                                       j["chapter"].as<std::string>());
            } else if (j.containsKey("position")) {
                place = Place::factory(j["position"].as<uint32_t>());
            } else {
                place = std::make_shared<NonePlace>();
                return false;
            }

            return true;
        }

        return false;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadablePlacemark &src, JsonVariant dst) -> bool;
