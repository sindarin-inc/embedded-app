#pragma once

#include "Misc/SpiramAllocator.hpp"
#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Place.hpp"
#include "Models/Readable.hpp"
#include "Models/UserReadablePartialChapterSummary.hpp"

class UserReadable : JsonSerializable {
public:
    UserReadable() = default;
    ~UserReadable() override = default;

    SpiramString uuid;
    std::shared_ptr<TxtPlace> txtPlace;
    std::shared_ptr<EPubPlace> ePubPlace;
    std::shared_ptr<UserReadablePartialChapterSummary> partialChapterSummary = nullptr;

    bool isArchived;
    std::shared_ptr<Readable> readable = nullptr;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("readable")) {
            log_w("Failed to parse UserReadable with JSON %s", DumpJson(j).c_str());
            return false;
        }

        uuid = j["uuid"].as<std::string>();

        if (j.containsKey("placemark")) {
            txtPlace =
                std::dynamic_pointer_cast<TxtPlace>(Place::factory(j["placemark"].as<uint32_t>()));
        } else {
            txtPlace = nullptr;
        }
        if (j.containsKey("chapter") && j.containsKey("chapter_offset")) {
            ePubPlace = std::dynamic_pointer_cast<EPubPlace>(
                Place::factory(j["chapter_offset"].as<uint32_t>(), j["chapter"].as<std::string>()));
        } else {
            ePubPlace = nullptr;
        }
        if (txtPlace->isFor(DocType::NONE) && ePubPlace->isFor(DocType::NONE)) {
            log_w("Failed to parse UserReadable with JSON: No Txt or EPub location detected.");
            return false;
        }

        if (j.containsKey("partial_chapter_summary")) {
            partialChapterSummary = SpiRamPtr<UserReadablePartialChapterSummary>();
            if (!partialChapterSummary->fromJson(j["partial_chapter_summary"])) {
                partialChapterSummary = nullptr;
            }
        } else {
            partialChapterSummary = nullptr;
        }

        isArchived = j["is_archived"].as<bool>();
        readable = SpiRamPtr<Readable>();

        return readable->fromJson(j["readable"]);
    }

    operator std::string() const { return std::string(uuid); }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadable &src, JsonVariant dst) -> bool;
