#pragma once

#include "Tokenizer.hpp"
#include "Tokens.hpp"

class EPubTokenizer : public Tokenizer {

public:
    EPubTokenizer(DisplaySystem &display) : Tokenizer(display) {}

    [[nodiscard]] auto loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens,
                                           bool backwards = false) -> StreamPlace override;

    [[nodiscard]] auto getPlaceAfterEndOfFileReached(StreamPlace streamPlace) const
        -> StreamPlace override {
        streamPlace.offset = 0;
        streamPlace.fileNumber += 1;
        return streamPlace;
    }

    // ToDo: To be adjusted related to the upcoming Place definition
    [[nodiscard]] auto toStreamPlace(const Place &place) const -> StreamPlace override {
        return StreamPlace{.offset = place, .fileNumber = 0};
    }

    [[nodiscard]] inline auto toPlace(const StreamPlace &streamPlace) const -> Place override {
        return streamPlace.offset;
    }

protected:
    auto getFileLocation(StreamPlace fromPlace) -> StreamPlace override;
};