#include "Renderer.hpp"

#include <algorithm>

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

    // std::cout << "Pager Line Count: " << pager_.getLineCount() << std::endl;
    // std::cout << "currentPlace: " << currentPlace_.str()
    //           << ", Pager first Place: " << pager_.getStreamPlaceOfFirstLine().str() <<
    //           std::endl;

    PROFILE_START(loadAndTokenizePage);

    // std::cout << "Pager Line Count: " << pager_.getLineCount() << std::endl;

    if ((pager_.getLineCount() == 0) || (!(currentPlace_ == pager_.getStreamPlaceOfFirstLine()))) {
        currentPlace_ = tokenizer_->loadAndTokenizePage(currentPlace_, tokens_, pager_);
    }
    nextPlace_ = pager_.getNextPageStreamPlace();

    PROFILE_END_LOG_MS(loadAndTokenizePage);

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

    // The following is done to compute the maximum number of characters a display
    // screen can get. The character "G" is a good compromize between smaller
    // ones (line lowercases) and bigger ones (like "M"). The obtained value added
    // to the maximum number of bytes in a paragraph will be used to return back in
    // the document to find the preceeding page start.

    uint16_t charWidth, charHeight;
    display_.getTextSize("G", &charWidth, &charHeight);

    uint16_t maxPageCount =
        ((display_.width() / charWidth) + 1) * ((display_.height() / charHeight) + 1);
    if (maxPageCount > MAX_BYTES_PER_DISPLAY_PAGE) {
        maxPageCount = MAX_BYTES_PER_DISPLAY_PAGE;
    }

    // std::cout << "Max Page Count: " << maxPageCount << std::endl;

    // Figure out how much to read before the current place. If we're too
    // close to the beginning, we just read from the beginning.

    StreamPlace seekPlace =
        tokenizer_->subtractOffset(fromPlace, maxPageCount + MAX_BYTES_PER_PARAGRAPH);

    // Tokenize it the same as you would for a forwards render. seekPlace will be returned to be at
    // a UTF8 character boundary

    if (!(seekPlace == fromPlace)) {

        seekPlace = tokenizer_->loadAndTokenizePage(
            seekPlace, tokens_, pager_, true,
            &fromPlace); // NOLINT(clang-analyzer-deadcode.DeadStores)

        // std::cout << "Line Count: " << pager_.getLineCount() << ", Token Count: " <<
        // tokens_.getCount()
        //           << std::endl;
        //  if (tokens_.getLast().streamPlace.offset == fromPlace.offset) {
        //      std::cout << "Last token point at fromPlace" << std::endl;
        //  } else {
        //      std::cout << "Last token doesn't point at fromPlace" << std::endl;
        //  }

        pager_.setFirstLineIdxBackward();

        // Idx firstLineIdx = pager_.getFirstLineIdx();
        // std::cout << "First line Idx of the page: " << firstLineIdx << std::endl;

        // Line line;
        // pager_.getLine(firstLineIdx, line);
        // std::cout << "First line: [" << line.data() << "]" << std::endl;
        // pager_.getLine(pager_.getLineCount() - 1, line);
        // std::cout << "Last line: [" << line.data() << "]" << std::endl;

        pager_.setNextPagePlace(fromPlace);
    } else {
        seekPlace = tokenizer_->loadAndTokenizePage(
            seekPlace, tokens_, pager_); // NOLINT(clang-analyzer-deadcode.DeadStores)
    }

    pager_.show();

    // std::cout << "Characters count in page: " << pager_.getPageLength() << std::endl;

    return pager_.getStreamPlaceOfFirstLine();
}

auto Renderer::getPreviousPlace() -> Place {

    PROFILE_START(goBackwardsOnePage);

    previousPlace_ = goBackwardsOnePage(currentPlace_);

    PROFILE_END_LOG_MS(goBackwardsOnePage);

    if constexpr (RENDERER_DEBUG) {
        log_w("Computed place: %s", previousPlace_.str().c_str());
    }
    return tokenizer_->toPlace(previousPlace_);
}