#ifndef FILE_RENDERER_H
#define FILE_RENDERER_H

#include <FS.h>

#include "Contents/Readable.hpp"
#include "Renderer.hpp"

#define MAX_CHARS_PER_PAGE 1500

class FileRenderer : public Renderer {
public:
    FileRenderer(DisplaySystem *displayInput, Readable &readableInput);

    auto getPreviousPlace() -> unsigned long;

    // Render a page of text with the current place
    // returns the next place for reading so the calling function can
    // track place.
    auto renderPageWithBox(unsigned long placeInFile) -> unsigned long;

    auto goBackwardsOnePage(unsigned long place) -> unsigned long;

protected:
    File fileHandle_;

    unsigned long currentPlace_ = 0;
    unsigned long previousPlace_ = 0;
    unsigned long nextPlace_ = 0;
    int firstGoAround_ = 1;

    char prevBuffer_[MAX_CHARS_PER_PAGE];
    char currBuffer_[MAX_CHARS_PER_PAGE];
    char nextBuffer_[MAX_CHARS_PER_PAGE];

    auto loadPrevIntoBuffer() -> unsigned long;
    auto loadCurrIntoBuffer() -> unsigned long;
    auto loadNextIntoBuffer() -> unsigned long;

    // This does the work of lineify from the old system. Loads one page of
    // text into that buffer given the current offset in the file
    auto loadIntoBuffer(char *buffer, unsigned long place) -> unsigned long;
};

#endif