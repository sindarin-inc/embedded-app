#pragma once

#include "Models/DocType.hpp"
#include "Tokenizer.hpp"

class NullTokenizer : public Tokenizer {

public:
    NullTokenizer(DisplaySystem &display, Tokens &tokens, Pager &pager)
        : Tokenizer(display, tokens, pager) {}
    ~NullTokenizer() override = default;

    [[nodiscard]] auto loadAndTokenizePage(StreamPlace fromPlace, bool backwards = false,
                                           StreamPlace *toPlace = nullptr)
        -> std::pair<StreamPlace, StreamPlace> override {
        error();
        return {};
    };

    // ToDo: To be adjusted related to the upcoming Place definition
    [[nodiscard]] auto toStreamPlace(const std::shared_ptr<Place> place) const
        -> StreamPlace override {
        error();
        return {};
    }

    [[nodiscard]] inline auto toPlace(const StreamPlace &streamPlace) const
        -> std::shared_ptr<Place> override {
        error();
        return std::make_shared<NonePlace>();
    }

    [[nodiscard]] auto streamSize() const -> size_t override {
        error();
        return 0;
    }

    [[nodiscard]] auto isFor(DocType docType) const -> bool override {
        error();
        return false;
    }

    void setStream(std::shared_ptr<RendererStream> stream) override { error(); }

    [[nodiscard]] auto getStream() -> std::shared_ptr<RendererStream> override { return nullptr; }

private:
    void error() const {
        log_e("There is no stream defined for the Renderer");
        REND_EXIT_SIM;
    }
};