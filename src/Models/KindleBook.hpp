#pragma once

#include <ArduinoJson.h>
#include <ctime>
#include <functional>
#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"

// Simple structure to match Kindle book format in kindle.json
class KindleBook : public JsonSerializable {
public:
    KindleBook() = default;
    ~KindleBook() override = default;

    std::string uuid; // Unique identifier for the book
    std::string title;
    std::string author;
    std::string progress;    // Progress as a string like "5%"
    std::string size;        // Size as a string like "1.8 MB" or "592.1 KB"
    int progressPercent = 0; // Parsed numeric progress (0-100)
    size_t sizeBytes = 0;    // Parsed size in bytes

    // Generate a shortened UUID for preferences
    [[nodiscard]] auto shortenedId() const -> std::string {
        // Return first 8 chars of title or UUID if present
        if (!uuid.empty()) {
            return uuid.substr(0, 8);
        }

        // Fall back to title-based ID if no UUID
        std::string id = "kb-";
        if (!title.empty()) {
            // Take first character and some hash of the title
            id += title.substr(0, 1);
            id += std::to_string(std::hash<std::string>{}(title) % 10000);
        } else {
            // Just use a timestamp-based hash if no title
            id += std::to_string(std::time(nullptr) % 10000);
        }
        return id;
    }

    auto fromJson(const JsonObject &j) -> bool override {
        uuid = j["uuid"].isNull() ? "" : j["uuid"].as<std::string>();
        title = j["title"].isNull() ? "" : j["title"].as<std::string>();
        author = j["author"].isNull() ? "" : j["author"].as<std::string>();
        progress = j["progress"].isNull() ? "" : j["progress"].as<std::string>();
        size = j["size"].isNull() ? "" : j["size"].as<std::string>();

        // Parse progress percentage
        if (!progress.empty()) {
            // Remove the '%' character and convert to int
            std::string progressStr = progress;
            if (progressStr.back() == '%') {
                progressStr.pop_back();
            }

            // Safe conversion without exceptions
            char *end;
            long val = strtol(progressStr.c_str(), &end, 10);
            if (end == progressStr.c_str() || *end != '\0' || val < 0 || val > 100) {
                // Conversion failed or value out of range
                progressPercent = 0;
            } else {
                progressPercent = static_cast<int>(val);
            }
        }

        // Parse size bytes
        if (!size.empty()) {
            // Check if size is in MB or KB
            float sizeValue = 0.0f;

            // Extract the numeric part
            size_t pos;
            if (size.find("MB") != std::string::npos) {
                // Try to parse the numeric part safely
                char *end;
                double val = strtod(size.c_str(), &end);
                pos = end - size.c_str();

                if (pos > 0 && val >= 0) {
                    sizeValue = static_cast<float>(val);
                    sizeBytes = static_cast<size_t>(sizeValue * 1024 * 1024);
                }
            } else if (size.find("KB") != std::string::npos) {
                // Try to parse the numeric part safely
                char *end;
                double val = strtod(size.c_str(), &end);
                pos = end - size.c_str();

                if (pos > 0 && val >= 0) {
                    sizeValue = static_cast<float>(val);
                    sizeBytes = static_cast<size_t>(sizeValue * 1024);
                }
            }
        }

        return true;
    }
};

// Vector type for KindleBook collections
using KindleBooksVector = SpiramVector<KindleBook>;

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const KindleBook &src, JsonVariant dst) -> bool;