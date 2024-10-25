#include "FileSystem.hpp"

#include "EPub/EPubFile.hpp"
#include "Misc/GUnzipStream.hpp"
#include "Misc/LookBackStream.hpp"
#include "sindarin-debug.h"

auto FileSystem::newRendererStream(const char *path) const -> std::shared_ptr<RendererStream> {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s", path);
        return nullptr;
    }
    return std::make_shared<ContainerStream<SFile>>(file, RendererStream::parseDocType(path));
}

auto FileSystem::newEPubRendererStream(const char *path, const std::shared_ptr<Readable> &readable,
                                       SolDrm &solDrm) const -> std::shared_ptr<EPubFile> {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s is the filename", path);
        return nullptr;
    }
    auto stream = std::make_shared<EPubFile>(file, readable, solDrm);
    if (!stream->isOpen()) {
        log_e("Failed to open stream for: %s is the filename", path);
        return nullptr;
    }

    return stream;
}

auto FileSystem::newGzipRendererStream(const char *path) const -> std::shared_ptr<RendererStream> {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s is the filename", path);
        return nullptr;
    }

    return std::make_shared<LookBackStream>(
        std::make_unique<GUnzipStream>(file, RendererStream::parseDocType(path)),
        LOOKBACK_BUFFER_SIZE, RendererStream::parseDocType(path));
}

auto FileSystem::deleteAll() -> bool { return removeDir(""); }