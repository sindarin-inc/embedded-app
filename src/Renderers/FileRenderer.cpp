#include "FileRenderer.hpp"

#include "Storage/FileSystem.hpp"
#include "sindarin-debug.h"

#define MAX_CHARS_PER_LINE 200

FileRenderer::FileRenderer(DisplaySystem &display, Readable &readableInput) : Renderer(display) {
    log_d("Attempting to open: %s", readableInput.path);
    fileHandle_ = FileSystem::getInstance()->open(readableInput.path);
    if (!fileHandle_) {
        log_e("Failed to open file: %s is the filename", readableInput.path);
        return;
    }

    currentPlace_ = readableInput.place;

    loaded();
}
