#include "FileSystem.hpp"

#include <EPubFile/EPubFile.hpp>
#include <Unzipper/UnzipperFileStream.hpp>

#include "Misc/GUnzipStream.hpp"
#include "Misc/LookBackStream.hpp"
#include "Storage/SFileUnzipperStream.hpp"
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

auto FileSystem::newEPubRendererStream(const char *path,
                                       const std::shared_ptr<SolDrm> &solDrm) const
    -> std::shared_ptr<EPubFileRendererStream> {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s", path);
        return nullptr;
    }
    auto unzipperStream = std::make_shared<SFileUnzipperStream>(file);
    auto epubFile = std::make_shared<EPubFile>(unzipperStream, solDrm);

    if (!epubFile->isOpen()) {
        log_e("Failed to open stream for: %s", path);
        return nullptr;
    }

    return std::make_shared<EPubFileRendererStream>(epubFile);
}

auto FileSystem::newGzipRendererStream(const char *path) const -> std::shared_ptr<RendererStream> {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s", path);
        return nullptr;
    }

    return std::make_shared<LookBackStream>(
        std::make_unique<GUnzipStream>(file, RendererStream::parseDocType(path)),
        LOOKBACK_BUFFER_SIZE, RendererStream::parseDocType(path));
}

auto FileSystem::deleteAll() -> bool { return removeDir(""); }