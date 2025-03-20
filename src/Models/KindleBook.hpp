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
            try {
                // Remove the '%' character and convert to int
                std::string progressStr = progress;
                if (progressStr.back() == '%') {
                    progressStr.pop_back();
                }
                progressPercent = std::stoi(progressStr);
            } catch (...) {
                progressPercent = 0;
            }
        }

        // Parse size bytes
        if (!size.empty()) {
            try {
                // Check if size is in MB or KB
                float sizeValue = 0.0f;
                if (size.find("MB") != std::string::npos) {
                    sizeValue = std::stof(size);
                    sizeBytes = static_cast<size_t>(sizeValue * 1024 * 1024);
                } else if (size.find("KB") != std::string::npos) {
                    sizeValue = std::stof(size);
                    sizeBytes = static_cast<size_t>(sizeValue * 1024);
                }
            } catch (...) {
                sizeBytes = 0;
            }
        }

        return true;
    }
};

// Vector type for KindleBook collections
using KindleBooksVector = SpiramVector<KindleBook>;

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const KindleBook &src, JsonVariant dst) -> bool;