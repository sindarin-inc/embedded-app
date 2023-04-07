#include "Firmware.hpp"

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const Firmware &src, JsonVariant dst) -> bool {
    dst["version"] = src.version;
    dst["url"] = src.version;
    dst["file_size"] = src.fileSize;
    dst["md5_hash"] = src.md5;
    dst["release_notes"] = src.releaseNotes;

    return true;
}
