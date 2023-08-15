#pragma once

#include <map>

#include "EPub/EPubFile.hpp"
#include "Tokenizer.hpp"

class EPubTokenizer : public Tokenizer {

public:
    EPubTokenizer(DisplaySystem &display, Tokens &tokens, Pager &pager)
        : Tokenizer(display, tokens, pager) {}
    ~EPubTokenizer() override = default;

    [[nodiscard]] auto loadAndTokenizePage(StreamPlace fromPlace, bool backwards = false,
                                           StreamPlace *toPlace = nullptr) -> StreamPlace override;

    [[nodiscard]] auto getPlaceAfterEndOfFileReached(StreamPlace streamPlace) const
        -> StreamPlace override {
        return {0, static_cast<uint16_t>(streamPlace.getFileNumber() + 1)};
    }

    [[nodiscard]] auto toStreamPlace(const std::shared_ptr<Place> place) const
        -> StreamPlace override {
        if (place->isA(PlaceType::EPUB)) {
            uint16_t spineIdx = stream_->getSpineIdxForOffset(place->getOffset());
            return {place->getOffset() - stream_->getSpine(spineIdx).offset, spineIdx};
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
        const EPubOpf::SpineItem &spineItem = stream_->getSpine(streamPlace.getFileNumber());
        return std::make_shared<EPubPlace>(streamPlace.getOffset() + spineItem.offset,
                                           spineItem.item->href);
    }

    [[nodiscard]] auto streamSize() const -> size_t override {
        return stream_.get() != nullptr ? stream_->size() : 0;
    }

    [[nodiscard]] auto isA(StreamType streamType) const -> bool override {
        return stream_->isA(streamType);
    }

    void setStream(std::shared_ptr<RendererStream> stream) override {
        stream_ = std::static_pointer_cast<EPubFile>(stream);
    }

private:
    std::shared_ptr<EPubFile> stream_;

    // clang-format off
    enum class Tag : uint8_t { NONE, BODY, P, LI, BREAK, H1, H2, H3, H4, H5, H6, 
                               B, I, A, IMG, IMAGE, EM, DIV, SPAN, PRE,
                               BLOCKQUOTE, STRONG, ANY, FONT_FACE, PAGE,
                               SUB, SUP };
    // clang-format on

    using Tags = std::map<std::string, Tag>;
    using Elements = std::map<std::string, const char *>;

    static Tags tags;
    static Elements elements;

    StreamPlace fromPlace_;
    StreamPlace *toPlace_;
    bool backwards_;
    bool started_;
    bool pageCompleted_;

    Idx prevNewParagraphIdx_;
    Idx lastStreamRelativeOffset_;

    [[nodiscard]] auto getCharEntity(const char *str, const char **newStrLocation) const -> const
        char *;
    [[nodiscard]] auto interpret(pugi::xml_node node) -> bool;
    [[nodiscard]] auto addStrToPage(const pugi::char_t *str, uint16_t len) -> bool;
};