#include "FileSystem.hpp"

#include "Contents/Json.hpp"
#include "Misc/StringUtil.hpp"
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
    auto error = deserializeJson(*manifest, manifestFile);
    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize manifest file: %s", error.c_str());
        manifestFile.close();
        // TODO: Return a real error
        return;
    }
    manifestFile.close();

    auto array = manifest->as<JsonArray>();
    readables->reserve(array.size());
    for (JsonVariant v : array) {
        auto readable = Readable(v["id"], v["name"], v["path"]);
        if (exists(readable.path)) {
            log_d("Found readable %s %s %s", readable.id.c_str(), readable.name.c_str(),
                  readable.path.c_str());
            readables->emplace_back(readable);
        } else {
            log_w("Couldn't find readable with path %s", readable.path.c_str());
        }
    }

    return;
}

void FileSystem::getDownloadedPocketReadables(std::vector<Readable> *readables) {
    std::vector<std::string> files;
    log_d("Listing pocket files");
    listDir("/pocket", files);
    for (auto &i : files) {
        auto id = i.substr(0, i.length() - 4);
        auto path = StringFormat("/pocket/%s", i.c_str());
        auto file = open(path.c_str());
        // Get first line (title)
        char title[100];
        file.readBytesUntil('\n', title, sizeof(title));
        file.close();
        Readable readable(id.c_str(), title, path.c_str());
        log_d("Found Pocket readable %s %s %s", readable.id.c_str(), readable.name.c_str(),
              readable.path.c_str());
        readables->push_back(readable);
    }
}

auto FileSystem::newRendererStream(const char *path) -> ContainerStream<File> * {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s is the filename", path);
        return nullptr;
    }
    return new ContainerStream<File>(file);
}
