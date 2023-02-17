#pragma once

#include <FS.h>
#include <string>

#include "Models/JsonSerializable.hpp"

enum BookmarkStatus { BOOKMARK_OFF = 0, BOOKMARK_ON = 1 };

struct LocalBookmark {
    uint32_t firstPlace;
    uint32_t lastPlace;
    BookmarkStatus status;
};

class ReadableBookmark : public JsonSerializable {
public:
    ReadableBookmark() = default;
    ~ReadableBookmark() override = default;

    std::string uuid;
    uint32_t startingPosition;
    uint32_t endingPosition;
    std::string readable;

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("name") || !j.containsKey("description")) {
            return false;
        }

        uuid = j["uuid"].as<std::string>();
        startingPosition = j["starting_position"].as<int>();
        endingPosition = j["ending_position"].as<int>();
        readable = j["readable"].as<std::string>();

        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableBookmark &src, JsonVariant dst) -> bool;
auto ReadOneBookmark(File &file, LocalBookmark &bookmark) -> bool;
