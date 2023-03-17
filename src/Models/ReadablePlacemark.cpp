#include "ReadablePlacemark.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadablePlacemark &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["position"] = src.position;
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, ReadablePlacemark &dst) {
    dst.uuid = src["uuid"].as<std::string>();
    dst.position = src["position"].as<uint32_t>();
}
