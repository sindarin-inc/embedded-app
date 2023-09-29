#pragma once

#include <array>
#include <map>

#include "EPub/EPubFile.hpp"
#include "Tokenizer.hpp"

// EPubTokenizer
//
// This class is responsible of transforming the content of a XHTML file into a stream of tokens,
// calling the Pager class when appropriate to transform the stream to a page displayable on the Sol
// Screen.
//
// For eBooks, chapters (that is, individual entries/XHTML files in the ePub opf's Spine structure)
// are always starting on a new blank page. For articles, there is no such distinction
//
// A subset of the HTML Tags is exploited by the tokenizer to render the document in a reasonable
// time scale. No CSS is present in the ePub (all CSS was removed by the server), the application
// uses some heuristic to decide how to present the information to the user. Some of these
// heuristics are described below.
//
// The XML parser in use is PugiXML (https://pugixml.org/). it's a small/fast parser written in C++
// that supply a tree structured projection of the document, with a variaty of methods that permits
// navigating the tree efficiently.
//
// To accomplish it's task, the tokenizer always start it's duty from the begining of the XHTML
// file. The main entry point is the loadAndTokenizePage(). It is normally called by the Renderer
// class. It is used for two purposes:
//
// - Forward page displacement: Starting at the beginning of the file, the method navigates toward
//   the requested *fromPlace* location offset. From there, it starts tokenizing the content
//   transmitting the information to the Pager class. The displacement to the target location is
//   fast as no tokenizing is done. The *fromPlace*, by convention, is the start of a page.
//
// - Backward page displacement: Starting at the beginning of the file, the method navigate toward
//   the requested *toPlace* location offset. That offset is the end location of the page to be
//   displayed. As the application doesn't keep a list of page locations, the method tokenizes and
//   build pages one at a time until the *toPlace* is reached. The last page built is then the one
//   to be displayed.
//
// TBC

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

    [[nodiscard]] inline auto isAnArticle() const -> bool override {
        return stream_->isAnArticle();
    }

private:
    std::shared_ptr<EPubFile> stream_;
    pugi::xml_node body_;

    // clang-format off

    // All the potential tags to do some work on. Not all of them are being processed at this point
    // in time. Some cleanup maybe required for the ones that are not used.    
    enum class Tag : uint8_t { P, LI, BREAK, H1, H2, H3, H4, H5, H6, 
                               B, I, A, IMG, PICTURE, SOURCE, EM, DIV, SPAN, PRE,
                               BLOCKQUOTE, STRONG, ANY, SUB, SUP, HR, OL, UL, 
                               HEADER, FIGURE, FIGCAPTION };
    // clang-format on

    using Tags = std::unordered_map<std::string, Tag>;
    using Entities = std::unordered_map<std::string, const char *>;
    using SuperscriptNumbers = std::array<const char *, 10>;

    // Hash HTML tags
    static Tags tags_;

    // Hash XHTML character entities (like &nbsp;) and their UTF-8 codification
    static Entities entities_;

    // Superscript Numbers UTF-8 codification
    static SuperscriptNumbers superscriptNumbers_;

    // Where the tokenizer will start to parse tokens
    StreamPlace fromPlace_;

    // Where it will end in case of backward movements
    StreamPlace *toPlace_;
    bool backwards_;

    // true when a <SUP></SUP> if being processed
    bool superscript_;

    // Used to add space qty that follow a token
    Token *lastToken_;

    // true if we are tokenizing/paging the content
    bool tokenizingStarted_;

    // true if at beginning of page and no tokens / lines where kept.
    bool beginningOfPage_;

    // true if the page is complete
    bool pageCompleted_;

    // used by the <a> processing to signify the need to add a ']' at the text's end.
    bool hyperlinkRightBracket_;

    // used for <h1>..<h6>
    bool startOfHeader_;

    // The offset where we are tokenizing
    uint32_t currentStreamOffset_;
    uint16_t currentFileIdx_;

    // Not null if there is a text token to be closed. This is required to take into account words
    // that could be containing tags like <b/> <i/> etc. These will be processed recursively, the
    // content potentially part of a word.
    Token *textToken_;
    uint16_t textCharCount_;

    static constexpr const uint8_t MaxLevelCount = 5;

    enum ListType { ORDERED, UNORDERED };
    uint16_t listSeqNbr_[MaxLevelCount];
    ListType listType_[MaxLevelCount];
    int8_t listLevel_;

    uint16_t savedLeftIndent_{};
    uint16_t savedBlockquoteLeftIndent_{};
    uint16_t savedBlockquoteRightIndent_{};

    static const constexpr char *levelChar_ = "-o-o-";

    // Could eventually be made smaller.
    // For now, used only for bracketing <a> tags pcdata
    char preText_[21];
    char postText_[21];

    // used to transform HTML character entities to their UTF8 char.
    char utf8Ch_[6];
    auto toUtf8(uint32_t val) -> char *;

    [[nodiscard]] auto getCharEntity(const char *str, const char **newStrLocation) -> const char *;
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

    // Called when it's time to close the last textToken, if there is any waiting to be closed.
    // Returns true if page is completed.
    auto closeTextToken() -> bool {
        if (textToken_ != nullptr) {
            tokens_.endOfCStr();
            lastToken_ = textToken_;
            addTokenToPage(*textToken_);
            textToken_ = nullptr;
            return pageCompleted_;
        } else {
            return false;
        }
    }

    void startNewPageWithToken(Token &token, const char *str, uint8_t strSize);

    auto addTokenToPage(Token &token, const char *str = nullptr, uint8_t strSize = 0) -> bool;
    auto findNodeEndOffset(pugi::xml_node node) -> uint32_t;
};