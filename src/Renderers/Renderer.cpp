#include "Renderer.hpp"

#include <algorithm>
#include <chrono>

// Show the (already loaded into buffer) current page
// and load the next page into the next buffer so we have it.
auto Renderer::renderPage(Place fromPlace, int16_t numSkipLines) -> Place {

    currentPlace_ = tokenizer_->toStreamPlace(fromPlace);

    if constexpr (RENDERER_DEBUG) {
        log_d("%s is place in file being requested", currentPlace_.str().c_str());
    }

    // This needs to happen before loading into buffer since it will affect what we load into the
    // buffer.
    display_.setFont(display_.style.getFontFamily().readableFont);

    tokenizer_->computeMaxTextTokenCharSize();

    PROFILE_START(loadCurrentPage);

    // std::cout << "Pager Line Count: " << pager_.getLineCount() << std::endl;
    // std::cout << "currentPlace: " << currentPlace_.str()
    //           << ", Pager first Place: " << pager_.getStreamPlaceOfFirstLine().str() <<
    //           std::endl;

    if constexpr (RENDERER_CHRONO) {
        auto start = std::chrono::high_resolution_clock::now();

        if ((pager_.getLineCount() == 0) ||
            (!(currentPlace_ == pager_.getStreamPlaceOfFirstLine()))) {
            currentPlace_ = tokenizer_->loadAndTokenizePage(currentPlace_, tokens_, pager_);
        }
        nextPlace_ = pager_.getNextPageStreamPlace();

        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        long long microseconds =
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        std::cout << "RENDERER TIME ELAPSED: " << microseconds << std::endl;
    } else {
        // std::cout << "Pager Line Count: " << pager_.getLineCount() << std::endl;

        if ((pager_.getLineCount() == 0) ||
            (!(currentPlace_ == pager_.getStreamPlaceOfFirstLine()))) {
            currentPlace_ = tokenizer_->loadAndTokenizePage(currentPlace_, tokens_, pager_);
        }
        nextPlace_ = pager_.getNextPageStreamPlace();
    }

    PROFILE_END_LOG_MS(loadCurrentPage);

    pager_.displayPage(numSkipLines);

    if constexpr (RENDERER_DEBUG) {
        log_w("Returning next page place to be %s", nextPlace_.str().c_str());
    }
    return tokenizer_->toPlace(nextPlace_);
}

// Seeks backwards and returns the place of one page previous.
auto Renderer::goBackwardsOnePage(StreamPlace fromPlace) -> StreamPlace {

    display_.setFont(display_.style.getFontFamily().readableFont);
    tokenizer_->computeMaxTextTokenCharSize();

    uint16_t displayWidth = display_.width();
    uint16_t displayHeight = display_.height();

    uint16_t charWidth, charHeight;
    display_.getTextSize("G", &charWidth, &charHeight);

    uint16_t maxPageCount = ((displayWidth / charWidth) + 1) * ((displayHeight / charHeight) + 1);

    // std::cout << "Max Page Count: " << maxPageCount << std::endl;

    // Step 1: Figure out how much to read before the current place. If we're too
    // close to the beginning, we just read from the beginning.
    StreamPlace seekPlace =
        tokenizer_->subtractOffset(fromPlace, maxPageCount + MAX_BYTES_PER_PARAGRAPH);

    // Step 2: Once the data is read into the buffer, tokenize it the same as you
    // would for a forwards render. seekPlace will be returned to be at a UTF8 character
    // boundary
    seekPlace = tokenizer_->loadAndTokenizePage(
        seekPlace, tokens_, pager_, true, &fromPlace); // NOLINT(clang-analyzer-deadcode.DeadStores)

    // pager_.show();

    // std::cout << "Line Count: " << pager_.getLineCount() << ", Token Count: " <<
    // tokens_.getCount()
    //           << std::endl;
    //  if (tokens_.getLast().streamPlace.offset == fromPlace.offset) {
    //      std::cout << "Last token point at fromPlace" << std::endl;
    //  } else {
    //      std::cout << "Last token doesn't point at fromPlace" << std::endl;
    //  }

    Idx firstLineIdx = pager_.getFirstLineIdxBackward();
    // std::cout << "First line Idx of the page: " << firstLineIdx << std::endl;

    // Line line;
    // pager_.getLine(firstLineIdx, line);
    // std::cout << "First line: [" << line.data() << "]" << std::endl;
    // pager_.getLine(pager_.getLineCount() - 1, line);
    // std::cout << "Last line: [" << line.data() << "]" << std::endl;

    pager_.setNextPagePlace_(fromPlace);

    // std::cout << "Characters count in page: " << pager_.getPageLength() << std::endl;

    return pager_.getStreamPlaceOfFirstLine();
}

auto Renderer::getPreviousPlace() -> Place {
    if constexpr (RENDERER_CHRONO) {
        auto start = std::chrono::high_resolution_clock::now();

        previousPlace_ = goBackwardsOnePage(currentPlace_);

        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        long long microseconds =
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        std::cout << "PREVIOUS PAGE TIME ELAPSED: " << microseconds << std::endl;
    } else {
        previousPlace_ = goBackwardsOnePage(currentPlace_);
    }

    if constexpr (RENDERER_DEBUG) {
        log_w("Computed place: %s", previousPlace_.str().c_str());
    }

    return tokenizer_->toPlace(previousPlace_);
}