#ifndef RAW_RENDERER_H
#define RAW_RENDERER_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "Contents/Readable.hpp"
#include "Renderer.hpp"

#define MAX_CHARS_PER_PAGE 1500

class RawRenderer : public Renderer {
public:
    RawRenderer(DisplaySystem *displayInput, std::string &string);

    void setString(std::string &string);

    // Render a page of text with the current place
    // returns the next place for reading so the calling function can
    // track place.
    auto renderPageWithBox(unsigned long placeInFile) -> unsigned long;

    auto getPreviousPlace() -> unsigned long;

protected:
    std::istringstream dataStream_;

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