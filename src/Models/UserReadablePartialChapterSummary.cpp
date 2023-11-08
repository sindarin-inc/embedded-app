#include "UserReadablePartialChapterSummary.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadablePartialChapterSummary &src, JsonVariant dst) -> bool {
    dst["text"] = src.text;
    dst["chapter"] = src.ePubPlace->getHRef();
    dst["chapter_offset"] = src.ePubPlace->getOffset();
    dst["read_pct"] = src.readPct;

    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, UserReadablePartialChapterSummary &dst) {
    dst.text = src["text"].as<std::string>();
    dst.ePubPlace = std::dynamic_pointer_cast<EPubPlace>(
        Place::factory(src["chapter_offset"].as<uint32_t>(), src["chapter"].as<std::string>()));
    dst.readPct = src["read_pct"].as<uint8_t>();
}
