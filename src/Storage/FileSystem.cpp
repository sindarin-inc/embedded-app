#include "FileSystem.hpp"

#include "Misc/GUnzipFile.hpp"
#include "sindarin-debug.h"

auto FileSystem::newRendererStream(const char *path) -> ContainerStream<File> * {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s", path);
        return nullptr;
    }
    return new ContainerStream<File>(file);
}

auto FileSystem::newGzipRendererStream(const char *path) -> ContainerStream<File> * {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s is the filename", path);
        return nullptr;
    }

    File gunzipFile = GUnzipFile::openGzFile(file);

    return new ContainerStream<File>(gunzipFile);
}