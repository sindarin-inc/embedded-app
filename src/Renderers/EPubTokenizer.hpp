#pragma once

#include <array>
#include <map>

#include "EPub/EPubFile.hpp"
#include "Tokenizer.hpp"

class EPubTokenizer : public Tokenizer {

public:
    EPubTokenizer(DisplaySystem &display, Tokens &tokens, Pager &pager)
        : Tokenizer(display, tokens, pager) {}
    ~EPubTokenizer() override = default;

    [[nodiscard]] auto loadAndTokenizePage(StreamPlace fromPlace, bool backwards = false,
                                           StreamPlace *toPlace = nullptr)
        -> std::pair<StreamPlace, StreamPlace> override;

    [[nodiscard]] auto getStreamPlaceFromTxtOffset(uint32_t txtOffset) const -> StreamPlace;

    [[nodiscard]] auto toStreamPlace(const std::shared_ptr<Place> place) const
        -> StreamPlace override;

    [[nodiscard]] inline auto toPlace(const StreamPlace &streamPlace) const
        -> std::shared_ptr<Place> override {
        const EPubOpf::SpineItem &spineItem = stream_->getSpine(streamPlace.getFileIdx());
        return std::make_shared<EPubPlace>(streamPlace.getOffset(), spineItem.item->href);
    }

    [[nodiscard]] auto streamSize() const -> size_t override {
        return stream_.get() != nullptr ? stream_->size() : 0;
    }

    [[nodiscard]] auto isFor(DocType docType) const -> bool override {
        return stream_->isFor(docType);
    }

    void setStream(std::shared_ptr<RendererStream> stream) override {
        stream_ = std::static_pointer_cast<EPubFile>(stream);
    }

    [[nodiscard]] auto computeStartingOffset(StreamPlace streamPlace) -> uint32_t;

    [[nodiscard]] auto isEmpty(pugi::xml_node node) -> bool;

    [[nodiscard]] auto goBackward(StreamPlace streamPlace, uint32_t offsetToSubtract)
        -> StreamPlace override;

private:
    std::shared_ptr<EPubFile> stream_;

    // clang-format off

    // All the potential tags to do some work on. Not all of them are being processed at this point
    // in time. Some cleanup maybe required for the ones that are not used.    
    enum class Tag : uint8_t { NONE, P, LI, BREAK, H1, H2, H3, H4, H5, H6, 
                               B, I, A, IMG, PICTURE, SOURCE, EM, DIV, SPAN, PRE,
                               BLOCKQUOTE, STRONG, ANY, FONT_FACE, PAGE,
                               SUB, SUP, HR, OL, UL, HEADER, FIGURE, FIGCAPTION };
    // clang-format on

    using Tags = std::unordered_map<std::string, Tag>;
    using Elements = std::unordered_map<std::string, const char *>;
    using SuperscriptNumbers = std::array<const char *, 10>;

    // Hash HTML tags
    static Tags tags;

    // Hash XHTML identity elements (like &nbsp;) and their UTF-8 codification
    static Elements elements;

    // Superscript Numbers UTF-8 codification
    static SuperscriptNumbers superscriptNumbers;

    // Where the tokenizer will start to parse tokens
    StreamPlace fromPlace_;

    // Where it will end in case of backward movements
    StreamPlace *toPlace_;
    bool backwards_;

    // true when a <SUP></SUP> if being processed
    bool superscript_;

    // Used to add space qty that follow a token
    Token *lastToken_;

    // true if we are past the fromPlace_ location
    bool started_;

    // true if the page is complete
    bool pageCompleted_;

    // Used to skip the paragraph following a picture. The paragraph is an anotation that could
    // be displayed if no picture are shown
    bool skipNextParagraph_;

    uint32_t currentStreamOffset_;

    static constexpr const uint8_t MaxLevelCount = 5;

    enum ListType { ORDERED, UNORDERED };
    uint16_t listSeqNbr_[MaxLevelCount];
    ListType listType_[MaxLevelCount];
    int8_t listLevel_;

    uint16_t savedLeftIndent_{};
    uint16_t savedBlockquoteLeftIndent_{};
    uint16_t savedBlockquoteRightIndent_{};

    static const constexpr char *levelChar_ = "-o-o-";

    [[nodiscard]] auto getCharEntity(const char *str, const char **newStrLocation) const -> const
        char *;
    [[nodiscard]] auto loadXHTMLAt(Idx fileIdx) -> pugi::xml_document &;
    [[nodiscard]] auto interpret(pugi::xml_node node) -> bool;
    [[nodiscard]] auto addStrToPage(const pugi::char_t *str, uint32_t len) -> bool;

    // [[nodiscard]] auto forwardToNextPageOffset(pugi::xml_node node) const -> uint32_t;

    void processPictureTag(pugi::xml_node node);

    [[nodiscard]] static inline auto isSpaces(const char *cstr) -> bool {
        while (*cstr) {
            if (!isspace(*cstr)) {
                if ((*cstr != '\xC2') || (*++cstr != '\xA0')) {
                    return false;
                }
            }
            cstr++;
        }
        return true;
    }

    auto addTokenToPage(Token &token, const char *str = nullptr, uint8_t strSize = 0) -> bool;
    auto findNodeEndOffset(pugi::xml_node node) -> uint32_t;
};