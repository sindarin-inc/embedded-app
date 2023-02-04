#include "FileSystem.hpp"

#include "Misc/GUnzipFile.hpp"
#include "Misc/GUnzipStream.hpp"
#include "Misc/LookBackStream.hpp"
#include "sindarin-debug.h"

auto FileSystem::newRendererStream(const char *path) -> RendererStream * {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s", path);
        return nullptr;
    }
    return new ContainerStream<File>(file);
}

auto FileSystem::newGzipRendererStream(const char *path) -> RendererStream * {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s is the filename", path);
        return nullptr;
    }

    //File gunzipFile = GUnzipFile::openGzFile(file);
    //return new ContainerStream<File>(gunzipFile);

    RendererStream* gs = new GUnzipStream(file);
    RendererStream* lgs = new LookBackStream(gs, 32768);

    return lgs;
}