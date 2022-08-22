#include "ReadableFile.hpp"

bool convertToJson(const ReadableFile &src, JsonVariant dst) {
    dst["id"] = src.id;
    dst["name"] = src.name;
    dst["size"] = src.size;
    dst["filetype"] = src.filetype;
    dst["uploaded_at"] = src.uploadedAt;
    dst["sha_hash"] = src.shaHash;
    dst["file"] = src.file;

    return true;
}