#pragma once

#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

class ReadableDictionary : public JsonSerializable {
public:
    ReadableDictionary() = default;
    ~ReadableDictionary() override = default;

    std::map<SpiramString, SpiramString> definitions;

    auto fromJson(const JsonObject &j) -> bool override {
        for (const auto &kvp : j) {
            SpiramString key = kvp.key().c_str();
            SpiramString value = kvp.value().as<const char *>();
            definitions.insert({key, value});
        }
        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableDictionary &src, JsonVariant dst) -> bool;
