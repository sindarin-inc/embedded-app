#pragma once

#include "Tokenizer.hpp"
#include "Tokens.hpp"

class EPubTokenizer : public Tokenizer {

public:
    EPubTokenizer(DisplaySystem &display) : Tokenizer(display) {}

    [[nodiscard]] auto loadAndTokenizePage(Place fromPlace, Tokens &tokens, bool backwards = false)
        -> Place override;

    [[nodiscard]] auto getPlaceAtBeginningOfFile(Place place) const -> Place override {
        place.offset = 0;
        return place;
    }

    [[nodiscard]] auto getPlaceAtBeginningOfDocument() const -> Place override {
        return Place{.offset = 0, .fileNumber = 1};
    }

    [[nodiscard]] auto getPlaceAfterEndOfFileReached(Place place) const -> Place override {
        place.offset = 0;
        place.fileNumber += 1;
        return place;
    }

    [[nodiscard]] auto atBeginningOfFile(Place place) const -> bool override {
        return place.offset == 0;
    }

    [[nodiscard]] auto addOffset(Place place, uint32_t offset) const -> Place override {
        place.offset += offset;
        return place;
    };

    [[nodiscard]] auto subtractOffset(Place place, uint32_t offset) const -> Place override {
        place.offset = (place.offset >= offset) ? place.offset - offset : 0;
        return place;
    }

protected:
    auto getFileLocation(Place fromPlace) -> Place override;
};