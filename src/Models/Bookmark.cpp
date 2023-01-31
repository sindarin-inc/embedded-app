#include "Bookmark.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Bookmark &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["starting_position"] = src.starting_position;
    dst["ending_position"] = src.ending_position;
    dst["readable"] = src.readable;
    dst["userReadable"] = src.userReadable;

    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, Bookmark &dst) {
    dst.uuid = src["uuid"].as<std::string>();
    dst.starting_position = src["starting_position"].as<int>();
    dst.ending_position = src["ending_position"].as<int>();
    dst.readable = src["readable"].as<std::string>();
    dst.userReadable = src["userReadable"].as<std::string>();
}
