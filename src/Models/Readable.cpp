#include "Readable.hpp"

#include "ReadableTableOfContentsChapter.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Readable &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["title"] = src.title;
    dst["date_created"] = src.dateCreated;
    dst["date_modified"] = src.dateModified;
    dst["date_published"] = src.datePublished;
    auto authorsJson = dst.createNestedArray("authors");
    for (const auto &author : src.authors) {
        authorsJson.add(author);
    }
    auto filesJson = dst.createNestedArray("files");
    for (const auto &file : src.files) {
        filesJson.add(file);
    }
    auto tocJson = dst.createNestedArray("toc");
    for (const auto &chapter : src.toc) {
        tocJson.add(chapter);
        // Check if any chapter has a summary, if so, set hasSummary
        if (!chapter.summary.empty()) {
            dst["has_summary"] = true;
        }
    }
    dst["readable_type"] = src.readableType;

    return true;
}
