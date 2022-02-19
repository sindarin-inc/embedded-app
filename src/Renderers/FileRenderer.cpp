#include "FileRenderer.hpp"

#include "Storage/FileSystem.hpp"
#include "sindarin-debug.h"

#define MAX_CHARS_PER_LINE 200

FileRenderer::FileRenderer(DisplaySystem &display, Readable &readableInput, FileSystem &fileSystem)
    : fileSystem(fileSystem), Renderer(display) {
    log_d("Attempting to open: %s", readableInput.path.c_str());
    fileHandle_ = fileSystem.open(readableInput.path);
    if (!fileHandle_) {
        log_e("Failed to open file: %s is the filename", readableInput.path.c_str());
        return;
    }

    currentPlace_ = readableInput.place;

    loaded();
}
