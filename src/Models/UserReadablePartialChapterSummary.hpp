#pragma once

#include <string>

#include "Misc/Defer.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/Place.hpp"
#include "Storage/FileSystem.hpp"

class UserReadablePartialChapterSummary : public JsonSerializable {
public:
    UserReadablePartialChapterSummary() = default;
    ~UserReadablePartialChapterSummary() override = default;

    static auto loadForReadable(FileSystem &fileSystem, const std::shared_ptr<Readable> &readable)
        -> std::shared_ptr<UserReadablePartialChapterSummary> {
        auto partialSummaryPath = readable->partialSummaryFilePath();
        auto file = fileSystem.open(partialSummaryPath, FILE_READ, false);
        // Check if partial summary already exists
        if (!file) {
            return nullptr;
        }
        defer { file.close(); };

        auto doc = std::make_unique<SpiRamJsonDocument>(file.size() * 2);
        auto error = deserializeJson(*doc, file);
        if (error.code() != DeserializationError::Ok) {
            log_e("Failed to deserialize partial summary file: %s", error.c_str());
            return nullptr;
        }

        auto partialSummary = std::make_shared<UserReadablePartialChapterSummary>();
        if (!partialSummary->fromJson(doc->as<JsonObject>())) {
            log_e("Failed to parse partial summary file: %s", partialSummaryPath.c_str());
            return nullptr;
        }

        return partialSummary;
    }

    SpiramString text;
    std::shared_ptr<EPubPlace> ePubPlace;
    uint8_t readPct{};

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("text") || !j.containsKey("chapter") ||
            !j.containsKey("chapter_offset") || !j.containsKey("read_pct")) {
            return false;
        }
        text = j["text"].as<std::string>();
        ePubPlace = std::dynamic_pointer_cast<EPubPlace>(
            Place::factory(j["chapter_offset"].as<uint32_t>(), j["chapter"].as<std::string>()));
        readPct = j["read_pct"].as<uint8_t>();

        if (text.length() == 0 || ePubPlace->isZero()) {
            return false;
        }

        return true;
    }

    auto operator==(const UserReadablePartialChapterSummary &other) const -> bool {
        return text == other.text && ePubPlace == other.ePubPlace && readPct == other.readPct;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const UserReadablePartialChapterSummary &src, JsonVariant dst) -> bool;
