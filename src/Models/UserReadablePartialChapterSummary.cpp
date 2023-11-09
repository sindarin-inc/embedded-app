#include "UserReadablePartialChapterSummary.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadablePartialChapterSummary &src, JsonVariant dst) -> bool {
    dst["text"] = src.text;
    if (src.ePubPlace != nullptr) {
        log_w("Serializing partial summary with no place");
        dst["chapter"] = src.ePubPlace->getHRef();
        dst["chapter_offset"] = src.ePubPlace->getOffset();
    }
    dst["read_pct"] = src.readPct;

    return true;
}
