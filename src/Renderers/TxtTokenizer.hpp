#pragma once

#include "Tokenizer.hpp"

class TxtTokenizer : public Tokenizer {

public:
    TxtTokenizer(DisplaySystem &display, Tokens &tokens, Pager &pager)
        : Tokenizer(display, tokens, pager) {}
    ~TxtTokenizer() override = default;

    [[nodiscard]] auto loadAndTokenizePage(StreamPlace fromPlace, bool backwards = false,
                                           StreamPlace *toPlace = nullptr) -> StreamPlace override;

    [[nodiscard]] auto getPlaceAfterEndOfFileReached(StreamPlace streamPlace) const
        -> StreamPlace override {
        return {};
    }

    // ToDo: To be adjusted related to the upcoming Place definition
    [[nodiscard]] auto toStreamPlace(const std::shared_ptr<Place> place) const
        -> StreamPlace override {
        if (place->isA(PlaceType::TXT)) {
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
        return std::make_shared<TxtPlace>(streamPlace.getOffset());
    }

    [[nodiscard]] auto streamSize() const -> size_t override {
        return stream_.get() != nullptr ? stream_->size() : 0;
    }

    [[nodiscard]] auto isA(StreamType streamType) const -> bool override {
        return stream_->isA(streamType);
    }

    void setStream(std::shared_ptr<RendererStream> stream) override { stream_ = stream; }

private:
    std::shared_ptr<RendererStream> stream_;

    [[nodiscard]] auto getFileLocation(StreamPlace fromPlace) -> StreamPlace;
};