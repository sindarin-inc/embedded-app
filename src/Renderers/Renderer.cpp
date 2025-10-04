#include "Renderer.hpp"

#include <algorithm>
#include <utility>

#include "UI/Fonts.hpp"

// Retrieve and show the page at fromPlace location.
auto Renderer::renderPage(std::shared_ptr<Place> fromPlace, int16_t numSkipLines, bool forceRefresh,
                          bool retrieveMenuParameters)
    -> std::pair<std::shared_ptr<Place>, uint8_t> {

    // if no fromPlace is received (null) or the place's DocType is wrong, get the first page
    // pointer, else convert fromPlace to a StreamPlace
    StreamPlace startPlace = (fromPlace == nullptr)
                                 ? tokenizer_->getPlaceAtBeginningOfDocument()
                                 : tokenizer_->toStreamPlace(std::move(fromPlace));

    REND_WLOG("%s is place in file being requested.", startPlace.str().c_str());

    tokens_.clear();
    pager_.clear();

    tokenizer_->computeMaxTextTokenCharSize();

    // std::cout << "Pager Line Count: " << pager_.getLineCount() << std::endl;
    // std::cout << "currentPlace: " << currentPlace_.str()
    //           << ", Pager first Place: " << pager_.getStreamPlaceOfFirstLine().str() <<
    //           std::endl;

    PROFILE_START(loadAndTokenizePage);

    // std::cout << "Pager Line Count: " << pager_.getLineCount() << std::endl;

    // We only re-tokenize if required (or forced through the forceRefresh param). May already been
    // done from the getPreviousPlace() method.

    if (pager_.linesIsEmpty() || (!(startPlace == pager_.getStreamPlaceOfFirstLine())) ||
        forceRefresh) {
        std::tie(currentPlace_, nextPlace_) =
            tokenizer_->loadAndTokenizePage(startPlace, false, nullptr, retrieveMenuParameters);

        if (retrieveMenuParameters && !pager_.getMenuParameters().empty()) {
            return {fromPlace, 0};
        }
    }

    PROFILE_END_LOG_MS(loadAndTokenizePage);

    // pager_.show();

    uint8_t darkPicturePercent = pager_.displayPage(numSkipLines);

    REND_WLOG("Returning next page place to be %s", nextPlace_.str().c_str());

    if constexpr (REND_STATS) {
        showStats();
    }

    return {tokenizer_->toPlace(nextPlace_), darkPicturePercent};
}

// Seeks backwards and returns the place of one page previous.
auto Renderer::goBackwardsOnePage(StreamPlace endPlace) -> StreamPlace {
    tokens_.clear();
    pager_.clear();

    tokenizer_->computeMaxTextTokenCharSize();

    // The following is done to compute the maximum number of characters a display
    // screen can get. The character "G" is a good compromize between smaller
    // ones (line lowercases) and bigger ones (like "M"). The obtained value added
    // to the maximum number of bytes in a paragraph will be used to return back in
    // the document to find the preceeding page start.

    uint16_t charWidth, charHeight;
    display_.getTextSize("G", &charWidth, &charHeight, font_);

    uint16_t maxPageCount = ((rect_.width / charWidth) + 1) * ((rect_.height / charHeight) + 1);
    if (maxPageCount > MAX_BYTES_PER_DISPLAY_PAGE) {
        maxPageCount = MAX_BYTES_PER_DISPLAY_PAGE;
    }

    // std::cout << "Max Page Count: " << maxPageCount << std::endl;

    // Figure out how much to read before the current place. If we're too
    // close to the beginning, we just read from the beginning.

    // log_e("Before goBackward(): endPlace: %s", endPlace.str().c_str());

    StreamPlace seekPlace =
        tokenizer_->goBackward(endPlace, maxPageCount + MAX_BYTES_PER_PARAGRAPH);

    // log_e("After goBackward(): seekPlace: %s", seekPlace.str().c_str());

    // Tokenize it the same as you would for a forwards render. seekPlace will be returned to be at
    // a UTF8 character boundary

    if (!(seekPlace == endPlace)) {
        StreamPlace lastPlace = endPlace;
        if (seekPlace.getSpineIdx() != endPlace.getSpineIdx()) {
            lastPlace = {END_OFFSET, seekPlace.getSpineIdx()};
        }
        std::tie(currentPlace_, nextPlace_) = tokenizer_->loadAndTokenizePage(
            seekPlace, true,
            &lastPlace); // NOLINT(clang-analyzer-deadcode.DeadStores)
    } else {
        std::tie(currentPlace_, nextPlace_) = tokenizer_->loadAndTokenizePage(
            seekPlace); // NOLINT(clang-analyzer-deadcode.DeadStores)
    }

    // pager_.show();

    // std::cout << "Characters count in page: " << pager_.getPageLength() << std::endl;

    if (currentPlace_.getOffset() == 0) {
        // We are now at the beginning of the current file. If the user seeked to some
        // position that is not the beginning of a page, this will insure than the
        // renderPage() method will rebuild the whole first page.
        pager_.clear();
    }

    return currentPlace_;
}

auto Renderer::getPreviousPlace() -> std::shared_ptr<Place> {

    PROFILE_START(goBackwardsOnePage);

    previousPlace_ = goBackwardsOnePage(currentPlace_);

    PROFILE_END_LOG_MS(goBackwardsOnePage);

    REND_WLOG("Computed place: %s", previousPlace_.str().c_str());

    return tokenizer_->toPlace(previousPlace_);
}
