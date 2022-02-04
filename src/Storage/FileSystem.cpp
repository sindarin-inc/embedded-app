#include "FileSystem.hpp"

#include "Contents/Json.hpp"
#include "sindarin-debug.h"

void FileSystem::getReadables(std::vector<Readable> *readables) {
#if !defined(DISABLE_SPIRAM)
    auto *manifest = new SpiRamJsonDocument(2048);
#else
    DynamicJsonDocument *manifest = new DynamicJsonDocument(2048);
#endif

    File manifestFile = this->open("/manifest.json");
    if (!manifestFile) {
        log_e("Failed to open manifest file.");
    }
    deserializeJson(*manifest, manifestFile);
    manifestFile.close();

    auto array = manifest->as<JsonArray>();
    readables->reserve(array.size());
    for (JsonVariant v : array) {
        auto readable = Readable(v["id"], v["name"], v["path"]);
        if (exists(readable.path)) {
            log_d("Found readable %s %s %s", readable.id, readable.name, readable.path);
            readables->emplace_back(readable);
        } else {
            log_w("Couldn't find readable with path %s", readable.path);
        }
    }

    return;
}
