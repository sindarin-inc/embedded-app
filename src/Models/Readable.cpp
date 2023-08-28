#include "Readable.hpp"

#include "ReadableTableOfContentsChapter.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Readable &src, JsonVariant dst) -> bool {
    dst["uuid"] = src.uuid;
    dst["title"] = src.title;
    dst["date_created"] = src.dateCreated;
    dst["date_modified"] = src.dateModified;
    dst["date_published"] = src.datePublished;
    dst["readable_type"] = src.readableType;
    auto authorsJson = dst.createNestedArray("authors");
    for (const auto &author : src.authors) {
        authorsJson.add(author);
    }
    auto filesJson = dst.createNestedArray("files");
    for (const auto &file : src.files) {
        filesJson.add(file);
    }

    // The toc is an empty array because we store it in a separate file but add it back in when
    // opening a Readable for reading
    dst.createNestedArray("toc");

    return true;
}
