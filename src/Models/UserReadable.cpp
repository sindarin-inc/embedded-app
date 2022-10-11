#include "UserReadable.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadable &src, JsonVariant dst) -> bool {
    dst["id"] = src.id;
    dst["readable"] = src.readable;

    return true;
}
