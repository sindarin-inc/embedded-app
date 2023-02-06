#include "FileSystem.hpp"

#include "Misc/GUnzipFile.hpp"
#include "Misc/GUnzipStream.hpp"
#include "Misc/LookBackStream.hpp"
#include "sindarin-debug.h"

#define LOOKBACK_BUFFER_SIZE 32768

auto FileSystem::newRendererStream(const char *path) -> RendererStream * {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s", path);
        return nullptr;
    }
    return new FileStream(file);
}

auto FileSystem::newGzipRendererStream(const char *path) -> RendererStream * {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s is the filename", path);
        return nullptr;
    }

    RendererStream *gs = new GUnzipStream(file);
    return new LookBackStream(gs, LOOKBACK_BUFFER_SIZE);
}