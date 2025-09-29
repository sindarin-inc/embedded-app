#pragma once

#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
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
    int progressCurrentPage = -1;    // Current page index from Kindle progress
    int progressTotalPages = -1;     // Total pages reported by Kindle progress
    bool isApprovedForDevice = true; // Whether the book is approved for this device
    SpiramString lastModified;       // Last modified timestamp

    static auto calculateProgressPercent(int currentPage, int totalPages) -> int {
        if (totalPages <= 0 || currentPage <= 0) {
            return 0;
        }
        int percentage = static_cast<int>(std::lround((static_cast<double>(currentPage) * 100.0) /
                                                      static_cast<double>(totalPages)));
        return std::clamp(percentage, 0, 100);
    }

    auto fromJson(const JsonObject &j) -> bool override {
        // Initialize progress fields (current page will be loaded from NVS)
        progressPercent = 0;
        progressCurrentPage = -1;
        progressTotalPages = -1;
        progress = "";

        uuid = j["uuid"].isNull() ? "" : j["uuid"].as<std::string>();
        shortenedUuid = uuid;
        shortenedUuid.resize(TRUNCATE_READABLE_UUID_FOR_PLACEMARK_BYTES);
        title = j["title"].isNull() ? "" : j["title"].as<std::string>();
        author = j["author"].isNull() ? "" : j["author"].as<std::string>();

        // Parse the new fields
        if (j.containsKey("is_approved_for_device")) {
            isApprovedForDevice = j["is_approved_for_device"].isNull()
                                      ? true
                                      : j["is_approved_for_device"].as<bool>();
        }

        if (j.containsKey("last_modified")) {
            lastModified = j["last_modified"].isNull() ? "" : j["last_modified"].as<std::string>();
        }

        // Only load total pages from JSON (current page is in NVS)
        if (j.containsKey("progress_total_pages") && !j["progress_total_pages"].isNull()) {
            progressTotalPages = j["progress_total_pages"];
        }

        return true;
    }
};

// Vector type for KindleBook collections
using KindleBooksVector = SpiramVector<KindleBook>;

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const KindleBook &src, JsonVariant dst) -> bool;
