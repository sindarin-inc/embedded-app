#include "ReadableFile.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const ReadableFile &src, JsonVariant dst) -> bool {
    dst["id"] = src.id;
    dst["name"] = src.name;
    dst["size"] = src.size;
    dst["filetype"] = src.filetype;
    dst["uploaded_at"] = src.uploadedAt;
    dst["sha_hash"] = src.shaHash;
    dst["file"] = src.file;

    return true;
}