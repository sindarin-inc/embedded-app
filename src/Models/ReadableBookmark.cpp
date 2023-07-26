#include "ReadableBookmark.hpp"

#include <regex>

#include "Models/Place.hpp"

// Simply reads from the bookmark file for this Readable and returns whether it was
// a turn on or turn off bookmark.
auto ReadOneBookmark(SFile &file, LocalBookmark &bookmark) -> bool {
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

    std::string bookmarkStr = file.readStringUntil('\n');
    std::regex regExStr("^(" PLACE_REGEX ")[ ]+(" PLACE_REGEX ")$");

    std::smatch sm;
    if (regex_match(bookmarkStr, sm, regExStr) && (sm.size() == 12)) {
        // log_d("First place: %s, last place: %s", sm[5].str().c_str(), sm[11].str().c_str());

        bookmark.firstPlace = Place::factory(sm[5].str());
        bookmark.lastPlace = Place::factory(sm[11].str());
        return true;
    }

    return false;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableBookmark &src, JsonVariant dst) -> bool {
    if (src.uuid.length() > 0) {
        dst["uuid"] = src.uuid;
    }
    dst["starting_position"] = src.startingPosition;
    dst["ending_position"] = src.endingPosition;
    dst["readable"] = src.readable;
    if (src.willDelete) {
        dst["status"] = 0;
    }

    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, ReadableBookmark &dst) {
    dst.uuid = src["uuid"].as<std::string>();
    dst.startingPosition = src["starting_position"].as<std::string>();
    dst.endingPosition = src["ending_position"].as<std::string>();
    dst.readable = src["readable"].as<std::string>();
}
