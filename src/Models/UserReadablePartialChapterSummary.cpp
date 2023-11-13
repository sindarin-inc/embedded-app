#include "UserReadablePartialChapterSummary.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadablePartialChapterSummary &src, JsonVariant dst) -> bool {
    if (src.ePubPlace == nullptr || src.text.length() == 0) {
        return false;
    }

    dst["text"] = src.text;
    dst["chapter"] = src.ePubPlace->getHRef();
    dst["chapter_offset"] = src.ePubPlace->getOffset();
    dst["read_pct"] = src.readPct;

    return true;
}
