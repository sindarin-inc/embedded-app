#include "Readable.hpp"

#include <EPubFile/EPubToc.hpp>

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Readable &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["title"] = src.title;
    dst["date_created"] = src.dateCreated;
    dst["date_modified"] = src.dateModified;
    dst["date_published"] = src.datePublished;
    dst["readable_type"] = src.readableType;
    dst["processed_by"] = src.processedBy;
    if (src.localOnly) {
        dst["local_only"] = true;
    }
    auto authorsJson = dst.createNestedArray("authors");
    for (const auto &author : src.authors) {
        authorsJson.add(author);
    }
    auto filesJson = dst.createNestedArray("files");
    for (const auto &file : src.files) {
        filesJson.add(file);
    }

    // The toc is an empty array because we store it in a separate file but add it back in when
    // opening the popup menu in a Readable while reading
    dst.createNestedArray("toc");

    return true;
}

// Returns the appropriate map for Telemetry::logMetric() calls.
auto Readable::telemetryPlacemarkMetrics() const
    -> std::unordered_map<std::string, TelemetryVariant> {
    if (place->isFor(DocType::TXT)) {
        return {{"po", place->getOffset()}};
    } else if (place->isFor(DocType::EPUB)) {
        return {{"po", place->getOffset()},
                {"pc", std::dynamic_pointer_cast<EPubPlace>(place)->getHRef()}};
    } else {
        return {{"po", 0U}};
    }
}
