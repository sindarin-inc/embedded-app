#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Models/Author.hpp"
#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Models/ReadableFile.hpp"

class Readable : JsonSerializable {
public:
    std::string id;
    std::string title;
    std::string dateCreated;
    std::string dateModified;
    std::string datePublished;
    std::vector<Author> authors;
    std::vector<ReadableFile> files;
    // TODO: source
    // TODO: categories

    // Local state
    uint32_t place{0}, placePrev{0};

    Readable() = default;

    auto file() const -> const ReadableFile & { return files[0]; }
    auto localFilePath() const -> std::string { return "/" + file().name; }
    auto remoteUrl() const -> std::string { return file().file; }

    auto fromJson(const JsonObject &j) -> bool override {
        if (!j.containsKey("id") || !j.containsKey("title") || !j.containsKey("date_created") ||
            !j.containsKey("date_modified") || !j.containsKey("date_published") ||
            !j.containsKey("authors") || !j.containsKey("files")) {
            log_w("Failed to parse Readable %s", DumpJson(j).c_str());
            return false;
        }
        id = j["id"].as<std::string>();
        title = j["title"].as<std::string>();
        dateCreated = j["date_created"].as<std::string>();
        dateModified = j["date_modified"].as<std::string>();
        datePublished = j["date_published"].as<std::string>();
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

bool convertToJson(const Readable &src, JsonVariant dst);
