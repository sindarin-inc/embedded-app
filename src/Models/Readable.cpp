#include "Readable.hpp"

bool convertToJson(const Readable &src, JsonVariant dst) {
    dst["id"] = src.id;
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

    return true;
}
