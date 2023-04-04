#include "ReadableArchive.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableArchive &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["is_archived"] = src.isArchived;
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, ReadableArchive &dst) {
    dst.uuid = src["uuid"].as<std::string>();
    dst.isArchived = src["is_archived"].as<bool>();
}
