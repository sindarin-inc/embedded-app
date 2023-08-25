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
                                           StreamPlace *toPlace = nullptr) -> StreamPlace override;

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

    [[nodiscard]] auto subtractOffset(StreamPlace streamPlace, uint32_t offset)
        -> StreamPlace override;

private:
    std::shared_ptr<EPubFile> stream_;

    // clang-format off

    // All the potential tags to do some work on. Not all of them are being processed at this point
    // in time. Some cleanup maybe required for the ones that are not used.    
    enum class Tag : uint8_t { NONE, P, LI, BREAK, H1, H2, H3, H4, H5, H6, 
                               B, I, A, IMG, PICTURE, SOURCE, EM, DIV, SPAN, PRE,
                               BLOCKQUOTE, STRONG, ANY, FONT_FACE, PAGE,
                               SUB, SUP };
    // clang-format on

    using Tags = std::map<std::string, Tag>;
    using Elements = std::map<std::string, const char *>;
    using SuperscriptNumbers = std::array<const char *, 10>;

    // Hash HTML tags
    static Tags tags;

    // Hash XHTML identity elements (like &nbsp;) and their UTF-8 codification
    static Elements elements;

    // Superscript Numbers UTF-8 codification
    static SuperscriptNumbers superscriptNumbers;

    // Where the tokenizer will start to parse tokens
    StreamPlace fromPlace_;

    // if back paging, the place to stop (will be the end of the previous page)
    StreamPlace *toPlace_;

    // true if doing a back page displacement
    bool backwards_;

    // true if we are past the fromPlace_ location
    bool started_;

    // true if the page is complete
    bool pageCompleted_;

    // Used to skip the paragraph following a picture. The paragraph is an anotation that could
    // be displayed if no picture are shown
    bool skipNextParagraph_;

    Idx prevNewParagraphIdx_;
    uint32_t firstOffset_;
    uint32_t lastStreamRelativeOffset_;

    [[nodiscard]] auto getCharEntity(const char *str, const char **newStrLocation) const -> const
        char *;
    [[nodiscard]] auto loadXHTMLAt(Idx fileIdx) -> pugi::xml_document &;
    [[nodiscard]] auto interpret(pugi::xml_node node) -> bool;
    [[nodiscard]] auto addStrToPage(const pugi::char_t *str, uint16_t len) -> bool;

    void processPictureTag(pugi::xml_node node);
    void processSuperscript(pugi::xml_node node);
};