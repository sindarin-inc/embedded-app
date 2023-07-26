#pragma once

#include <Storage/SFile.hpp>
#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Place.hpp"

enum BookmarkStatus { BOOKMARK_OFF = 0, BOOKMARK_ON = 1 };

struct LocalBookmark {
    std::shared_ptr<Place> firstPlace;
    std::shared_ptr<Place> lastPlace;
    BookmarkStatus status;
};

class ReadableBookmark : public JsonSerializable {
public:
    ReadableBookmark() = default;
    ~ReadableBookmark() override = default;

    std::string uuid;
    std::string startingPosition;
    std::string endingPosition;
    SpiramString readable;
    bool willDelete;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("name") || !j.containsKey("description")) {
            return false;
        }

        uuid = j["uuid"].as<std::string>();
        startingPosition = j["starting_position"].as<std::string>();
        endingPosition = j["ending_position"].as<std::string>();
        readable = j["readable"].as<std::string>();

        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableBookmark &src, JsonVariant dst) -> bool;
auto ReadOneBookmark(SFile &file, LocalBookmark &bookmark) -> bool;
