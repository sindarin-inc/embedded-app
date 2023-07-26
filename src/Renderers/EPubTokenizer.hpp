#pragma once

#include "Tokenizer.hpp"

class EPubTokenizer : public Tokenizer {

public:
    EPubTokenizer(DisplaySystem &display) : Tokenizer(display) {}

    [[nodiscard]] auto loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens, Pager &pager,
                                           bool backwards = false, StreamPlace *toPlace = nullptr)
        -> StreamPlace override;

    [[nodiscard]] auto getPlaceAfterEndOfFileReached(StreamPlace streamPlace) const
        -> StreamPlace override {
        return {0, static_cast<uint16_t>(streamPlace.getFileNumber() + 1)};
    }

    // ToDo: To be adjusted related to the upcoming Place definition
    [[nodiscard]] auto toStreamPlace(const std::shared_ptr<Place> place) const
        -> StreamPlace override {
        if (place->isA(PlaceType::EPUB)) {
            return {place->getOffset(), 0};
        } else {
            log_e("Wrong Place Type: %" PRIu8, static_cast<uint8_t>(place->getPlaceType()));
            if constexpr (ON_ERROR_EXIT) {
                exit(1);
            }
        }

        return {};
    }

    [[nodiscard]] inline auto toPlace(const StreamPlace &streamPlace) const
        -> std::shared_ptr<Place> override {
        return std::make_shared<EPubPlace>(streamPlace.getOffset(), "");
    }

protected:
    auto getFileLocation(StreamPlace fromPlace) -> StreamPlace override;
};