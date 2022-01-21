#ifndef FILE_RENDERER_H
#define FILE_RENDERER_H

#include <FS.h>

#include "Contents/Readable.hpp"
#include "Renderer.hpp"

class FileRenderer : public Renderer {
public:
    FileRenderer(DisplaySystem &display, Readable &readableInput);

    auto available() -> int override { return fileHandle_.available(); }
    auto read() -> int override { return fileHandle_.read(); };
    auto seek(uint32_t pos) -> bool override { return fileHandle_.seek(pos); };
    auto position() -> size_t override { return fileHandle_.position(); }

protected:
    File fileHandle_;
};

#endif
