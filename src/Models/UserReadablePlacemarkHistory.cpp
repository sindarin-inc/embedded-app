#include "Models/UserReadablePlacemarkHistory.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadablePlacemarkHistory &src, JsonVariant dst) -> bool {
    if (src.ePubPlace == nullptr) {
        return false;
    }

    dst["device_sn"] = src.deviceSn;
    dst["chapter"] = src.ePubPlace->getHRef();
    dst["chapter_offset"] = src.ePubPlace->getOffset();
    dst["placemark"] = src.placemark;
    dst["date_created"] = src.dateCreated;

    return true;
}
