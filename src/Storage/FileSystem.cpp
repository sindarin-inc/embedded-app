#include "FileSystem.hpp"

#include "Misc/GUnzipStream.hpp"
#include "Misc/LookBackStream.hpp"
#include "sindarin-debug.h"

auto FileSystem::newRendererStream(const char *path) -> std::shared_ptr<RendererStream> {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s", path);
        return nullptr;
    }
    return std::make_shared<ContainerStream<SFile>>(file, RendererStream::parseStreamType(path));
}

auto FileSystem::newGzipRendererStream(const char *path) -> std::shared_ptr<RendererStream> {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s is the filename", path);
        return nullptr;
    }

    return std::make_shared<LookBackStream>(
        std::make_unique<GUnzipStream>(file, RendererStream::parseStreamType(path)),
        LOOKBACK_BUFFER_SIZE, RendererStream::parseStreamType(path));
}

auto FileSystem::deleteAll() -> bool { return removeDir(""); }