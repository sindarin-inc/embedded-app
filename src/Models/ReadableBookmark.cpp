#include "ReadableBookmark.hpp"

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

    // The Place::factory() will parse up to the ']' character, so no need to find the end of the
    // place string; This also allow for transition of the old offset value as it will be
    // translated to an TxtPlace through the factory() method.
    const char *s;
    s = bookmarkStr.c_str();
    while (*s == ' ') {
        s++;
    }
    if (*s) {
        bookmark.firstPlace = Place::factory(s);

        if (*s == '[') {
            s++;
            while (*s && (*s != '[')) {
                s++;
            }
        } else {
            while (*s && (*s != ' ')) {
                s++;
            }
            while (*s == ' ') {
                s++;
            }
        }

        if (*s) {
            bookmark.lastPlace = Place::factory(s);
            return true;
        }
    }

    return false;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableBookmark &src, JsonVariant dst) -> bool {
    if (src.uuid.length() > 0) {
        dst["uuid"] = src.uuid;
    }
    if (src.chapter.length() > 0) {
        dst["chapter"] = src.chapter;
        dst["chapter_offset"] = src.chapterOffset;
    } else if (src.startingPosition) {
        dst["starting_position"] = src.startingPosition;
        dst["ending_position"] = src.endingPosition;
    }
    dst["readable"] = src.readable;
    if (src.willDelete) {
        dst["status"] = 0;
    }

    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, ReadableBookmark &dst) {
    dst.uuid = src["uuid"].as<std::string>();
    dst.chapter = src["chapter"].as<std::string>();
    dst.chapterOffset = src["chapter_offset"].as<uint32_t>();
    dst.startingPosition = src["starting_position"].as<uint32_t>();
    dst.endingPosition = src["ending_position"].as<uint32_t>();
    dst.readable = src["readable"].as<std::string>();
}
