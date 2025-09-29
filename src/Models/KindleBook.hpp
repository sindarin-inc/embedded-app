#pragma once

#include <ArduinoJson.h>
#include <ctime>
#include <functional>
#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Readable.hpp"

// Simple structure to match Kindle book format in kindle.json
class KindleBook : public JsonSerializable {
public:
    KindleBook() = default;
    ~KindleBook() override = default;

    SpiramString uuid; // Unique identifier for the book
    SpiramString shortenedUuid;
    SpiramString title;
    SpiramString author;
    SpiramString progress;           // Progress as a string like "5%"
    int progressPercent = 0;         // Parsed numeric progress (0-100)
    bool isApprovedForDevice = true; // Whether the book is approved for this device
    SpiramString lastModified;       // Last modified timestamp

    auto fromJson(const JsonObject &j) -> bool override {
        uuid = j["uuid"].isNull() ? "" : j["uuid"].as<std::string>();
        shortenedUuid = uuid;
        shortenedUuid.resize(TRUNCATE_READABLE_UUID_FOR_PLACEMARK_BYTES);
        title = j["title"].isNull() ? "" : j["title"].as<std::string>();
        author = j["author"].isNull() ? "" : j["author"].as<std::string>();
        progress = j["progress"].isNull() ? "" : j["progress"].as<std::string>();

        // Parse the new fields
        if (j.containsKey("is_approved_for_device")) {
            isApprovedForDevice = j["is_approved_for_device"].isNull()
                                      ? true
                                      : j["is_approved_for_device"].as<bool>();
        }

        if (j.containsKey("last_modified")) {
            lastModified = j["last_modified"].isNull() ? "" : j["last_modified"].as<std::string>();
        }

        // Parse progress percentage
        if (!progress.empty()) {
            // Remove the '%' character and convert to int
            auto progressStr = progress;
            if (progressStr.back() == '%') {
                progressStr.pop_back();
            }
            char *end;
            long val = strtol(progressStr.c_str(), &end, 10);
            if (end == progressStr.c_str() || *end != '\0' || val < 0 || val > 100) {
                // Conversion failed or value out of range
                progressPercent = 0;
            } else {
                progressPercent = static_cast<int>(val);
            }
        }

        return true;
    }
};

// Vector type for KindleBook collections
using KindleBooksVector = SpiramVector<KindleBook>;

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const KindleBook &src, JsonVariant dst) -> bool;