#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Models/Author.hpp"
#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/ReadableFile.hpp"

#define TRUNCATE_READABLE_UUID_FOR_PLACEMARK_BYTES 8

class Readable : JsonSerializable {
public:
    Readable() = default;
    ~Readable() override = default;

    std::string uuid;
    std::string placemarkUuid;
    std::string title;
    std::string dateCreated;
    std::string dateModified;
    std::string datePublished;
    std::vector<Author> authors;
    std::vector<ReadableFile> files;
    // TODO: source
    // TODO: categories

    //! Set to true if this is a readable side-loaded onto the device (that doesn't come from the
    //! server)
    bool localOnly = false;
    // Local state
    uint32_t place{0}, placePrev{0};

    [[nodiscard]] auto file() const -> const ReadableFile & { return files[0]; }
    [[nodiscard]] auto localFilePath() const -> std::string { return "/" + file().name; }
    [[nodiscard]] auto remoteUrl() const -> std::string { return file().file; }

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("uuid") || !j.containsKey("title") || !j.containsKey("date_created") ||
            !j.containsKey("date_modified") || !j.containsKey("authors") ||
            !j.containsKey("files")) {
            log_w("Failed to parse Readable %s", DumpJson(j).c_str());
            return false;
        }
        uuid = j["uuid"].as<std::string>();
        placemarkUuid = j["uuid"].as<std::string>();
        placemarkUuid.resize(TRUNCATE_READABLE_UUID_FOR_PLACEMARK_BYTES);
        title = j["title"].as<std::string>();
        dateCreated = j["date_created"].as<std::string>();
        dateModified = j["date_modified"].as<std::string>();
        datePublished = j["date_published"].as<std::string>();
        localOnly = j["local_only"].as<bool>();
        authors.clear();
        for (const auto &author : j["authors"].as<JsonArray>()) {
            authors.emplace_back();
            authors.back().fromJson(author);
        }
        files.clear();
        for (const auto &file : j["files"].as<JsonArray>()) {
            files.emplace_back();
            files.back().fromJson(file);
        }
        return true;
    }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Readable &src, JsonVariant dst) -> bool;
