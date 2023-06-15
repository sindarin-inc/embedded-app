#include "UserReadable.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadable &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["placemark"] = src.placemark;
    dst["is_archived"] = src.isArchived;
    dst["readable"] = *src.readable;

    return true;
}
