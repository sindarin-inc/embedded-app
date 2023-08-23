#include "ReadableDictionary.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableDictionary &src, JsonVariant dst) -> bool {

    JsonObject dictObj = dst.createNestedObject("dictionary");
    for (const auto &kvp : src.definitions) {
        dictObj[kvp.first.c_str()] = kvp.second.c_str();
    }

    return true;
}
