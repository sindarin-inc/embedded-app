#ifndef RAW_RENDERER_H
#define RAW_RENDERER_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "Contents/Readable.hpp"
#include "Renderer.hpp"

#define MAX_CHARS_PER_PAGE 1500
#define PAGE_READ 1
#define SCREED_READ 2

class RawRenderer : public Renderer {
public:
    RawRenderer(DisplaySystem *displayInput, std::string &string);

    void setString(std::string &string);

    void setReadMode(int newMode);
    auto getReadMode() -> int;
    void flipReadMode();

    void increaseSpeed();
    void decreaseSpeed();
    void setSpeed(int newSpeed);

    // Always call this on each loop.
    // If not screed, it'll be a nop
    auto renderContinueScreed() -> unsigned long;

    // Render a page of text with the current place
    // returns the next place for reading so the calling function can
    // track place.
    auto renderPageWithBox(unsigned long placeInFile) -> unsigned long;

    auto getPreviousPlace() -> unsigned long;

protected:
    std::istringstream dataStream_;

    unsigned long screedLastTime_;

    int screedSpeed_;

    int readMode_;

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