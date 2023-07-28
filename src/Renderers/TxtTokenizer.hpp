#pragma once

#include "Tokenizer.hpp"

class TxtTokenizer : public Tokenizer {

public:
    TxtTokenizer(DisplaySystem &display) : Tokenizer(display) {}

    [[nodiscard]] auto loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens, Pager &pager,
                                           bool backwards = false, StreamPlace *toPlace = nullptr)
        -> StreamPlace override;

    [[nodiscard]] auto getPlaceAfterEndOfFileReached(StreamPlace streamPlace) const
        -> StreamPlace override {
        streamPlace.offset = 0;
        return streamPlace;
    }

    // ToDo: To be adjusted related to the upcoming Place definition
    [[nodiscard]] auto toStreamPlace(const Place &place) const -> StreamPlace override {
        return StreamPlace{.offset = place, .fileNumber = 0};
    }

    [[nodiscard]] inline auto toPlace(const StreamPlace &streamPlace) const -> Place override {
        return streamPlace.offset;
    }

private:
    [[nodiscard]] auto getFileLocation(StreamPlace fromPlace) -> StreamPlace override;
};