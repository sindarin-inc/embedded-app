#pragma once

#include "Tokenizer.hpp"

class NullTokenizer : public Tokenizer {

public:
    NullTokenizer(DisplaySystem &display) : Tokenizer(display) {}

    [[nodiscard]] auto loadAndTokenizePage(StreamPlace fromPlace, Tokens &tokens, Pager &pager,
                                           bool backwards = false, StreamPlace *toPlace = nullptr)
        -> StreamPlace override {
        error();
        return StreamPlace();
    };

    [[nodiscard]] auto getPlaceAfterEndOfFileReached(StreamPlace streamPlace) const
        -> StreamPlace override {
        error();
        return StreamPlace();
    }

    // ToDo: To be adjusted related to the upcoming Place definition
    [[nodiscard]] auto toStreamPlace(const Place &place) const -> StreamPlace override {
        error();
        return StreamPlace();
    }

    [[nodiscard]] inline auto toPlace(const StreamPlace &streamPlace) const -> std::shared_ptr<Place> override {
        error();
        return std::make_shared<NonePlace>();
    }

protected:
    auto getFileLocation(StreamPlace fromPlace) -> StreamPlace override {
        error();
        return fromPlace;
    };

private:
    void error() const {
        log_e("There is no stream defined for the Rendere!!!");
        if constexpr (ON_ERROR_EXIT) {
            exit(1);
        }
    }
};