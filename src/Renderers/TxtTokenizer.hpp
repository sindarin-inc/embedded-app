#pragma once

#include "Tokenizer.hpp"

class TxtTokenizer : public Tokenizer {

public:
    TxtTokenizer(DisplaySystem &display, Tokens &tokens, Pager &pager)
        : Tokenizer(display, tokens, pager) {}
    ~TxtTokenizer() override = default;

    [[nodiscard]] auto loadAndTokenizePage(StreamPlace fromPlace, bool backwards = false,
                                           StreamPlace *toPlace = nullptr)
        -> std::pair<StreamPlace, StreamPlace> override;

    // ToDo: May have to be adjusted related to the upcoming Place definition
    [[nodiscard]] auto toStreamPlace(const std::shared_ptr<Place> place) const
        -> StreamPlace override {
        if (place->isFor(DocType::TXT)) {
            return {place->getOffset(), 0};
        } else {
            log_e("Wrong Place Type: %" PRIu8, static_cast<uint8_t>(place->getDocType()));
            REND_EXIT_SIM;
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

    [[nodiscard]] auto isFor(DocType docType) const -> bool override {
        return stream_->isFor(docType);
    }

    void setStream(std::shared_ptr<RendererStream> stream) override { stream_ = stream; }

    [[nodiscard]] auto getStream() -> std::shared_ptr<RendererStream> override { return stream_; }

    [[nodiscard]] inline auto isReady() const -> bool override { return stream_.get() != nullptr; }

    [[nodiscard]] inline auto getTxtOffset() const -> uint32_t override {
        return stream_->position();
    }

private:
    std::shared_ptr<RendererStream> stream_;

    // Used to add space qty that follow a token
    Token *lastToken_;

    [[nodiscard]] auto getFileLocation(StreamPlace fromPlace) -> StreamPlace;

    // Integrates the token to the tokens_[] array and to the page.
    // Returns true if the page is ready.
    inline auto addTokenToPage(Token &token, bool backwards = false, const char *str = nullptr,
                               uint8_t strSize = 0) -> bool {
        if (tokens_.add(token, str, strSize)) {
            // log_w("Token Id: %" PRIu8 " at idx: %" PRIu16, (uint8_t)token.tokenId,
            // tokens_.getLastIdx());
            return !pager_.addLastTokenToPage(backwards) && !backwards;
        } else {
            return false;
        }
    }
};
