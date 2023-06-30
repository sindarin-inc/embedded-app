#include "UserReadablePartialChapterSummary.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadablePartialChapterSummary &src, JsonVariant dst) -> bool {
    dst["text"] = src.text;
    dst["placemark"] = src.placemark;
    dst["read_pct"] = src.read_pct;

    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, UserReadablePartialChapterSummary &dst) {
    dst.text = src["text"].as<std::string>();
    dst.placemark = src["placemark"].as<uint32_t>();
    dst.read_pct = src["read_pct"].as<uint8_t>();
}
