#include "ReadableDictionary.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableDictionary &src, JsonVariant dst) -> bool {
    dst["w"] = src.word;
    dst["d"] = src.definition;
    auto locations = dst.createNestedArray("l");
    for (const auto &location : src.locations) {
        locations.add(location);
    }

    return true;
}
