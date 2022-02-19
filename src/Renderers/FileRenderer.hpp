#ifndef FILE_RENDERER_H
#define FILE_RENDERER_H

#include <FS.h>

#include "Contents/Readable.hpp"
#include "Renderer.hpp"
#include "Storage/FileSystem.hpp"

class FileRenderer : public Renderer {
public:
    FileRenderer(DisplaySystem &display, Readable &readableInput, FileSystem &fileSystem);

    auto available() -> int override { return fileHandle_.available(); }
    auto read() -> int override { return fileHandle_.read(); };
    auto seek(uint32_t pos) -> bool override { return fileHandle_.seek(pos); };
    auto position() -> size_t override { return fileHandle_.position(); }
    auto size() -> size_t override { return fileHandle_.size(); }

    FileSystem &fileSystem;

protected:
    File fileHandle_;
};

#endif
