#include "ReadableBookmark.hpp"

// Simply reads from the bookmark file for this Readable and returns whether it was
// a turn on or turn off bookmark.
auto readOneBookmark(File &file, Bookmark &bookmark) -> bool {
    // The first character in the line is a + or -, read that first
    if (!file.available()) {
        return false;
    }

    char firstChar = file.read();
    if (firstChar == '+') {
        bookmark.status = BOOKMARK_ON;
    } else if (firstChar == '-') {
        bookmark.status = BOOKMARK_OFF;
    } else {
        // Got an unexpected character
        return false;
    }

    // Next will be a space.
    char secondChar = file.read();
    if (secondChar != ' ') {
        return false;
    }

    String firstThing = file.readStringUntil(' ');
    bookmark.firstPlace = firstThing.toInt();

    String secondThing = file.readStringUntil('\n');
    bookmark.lastPlace = secondThing.toInt();
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableBookmark &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["starting_position"] = src.starting_position;
    dst["ending_position"] = src.ending_position;
    dst["readable"] = src.readable;

    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, ReadableBookmark &dst) {
    dst.uuid = src["uuid"].as<std::string>();
    dst.starting_position = src["starting_position"].as<int>();
    dst.ending_position = src["ending_position"].as<int>();
    dst.readable = src["readable"].as<std::string>();
}
