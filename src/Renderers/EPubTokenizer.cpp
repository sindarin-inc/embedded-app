#include "EPubTokenizer.hpp"

//
// This loads from the stream character by character and populates
// the token buffer based on the TOKEN enum.
// After this is done, then we can do the heavy lifting of walking
// through the tokens, checking size, and rendering the text on
// a line by line basis.
//
// We track backwards because we need to double the amount read in a backwards
// read (2 pages instead of 1)
//
// Computes the number of word in the page.
//
// Returns the computed place where the page start really.
//
// IMPORTANT: The getFileLocation() method must be called only through this method.
auto EPubTokenizer::loadAndTokenizePage(Place fromPlace, Tokens &tokens, bool backwards) -> Place {
    return fromPlace;
}

// Must be called only by the loadAndTokenizePage().
// From the fromPlace variable, seek into the file and synchronize
// the value at a valid UTF8 location. An UTF8 character start with
// a first byte MSBit == 0 or the two MSBits == 11
//
// returns an updated fromPlace location or the LOCATION_ERROR value.
auto EPubTokenizer::getFileLocation(Place fromPlace) -> Place {

    // if received place is larger than the stream size, reset it to
    // the beginning of file
    if (fromPlace.offset >= stream_->size()) {
        fromPlace = getPlaceAtBeginningOfDocument();
    }

    if (!stream_->seek(fromPlace.offset)) {
        log_e("failed to seek at place %" PRIu32 " in file!", fromPlace);
        if constexpr (ON_ERROR_EXIT) {
            exit(2);
        }
    } else {
        while (stream_->available()) {
            uint8_t ch = stream_->read();
            // A valid UTF8 character is ASCII or got it's first byte
            // larger or equal to 0xC0
            if ((ch < 0x7FU) || (ch >= 0xC0U)) {
                stream_->seek(fromPlace.offset);
                return fromPlace;
            }
            fromPlace.offset += 1;
            if constexpr (RENDERER_DEBUG) {
                log_d("Synching to start of UTF8 character at place %s", fromPlace.str().c_str());
            }
        }
    }

    fromPlace.offset = LOCATION_ERROR;
    return fromPlace;
}
