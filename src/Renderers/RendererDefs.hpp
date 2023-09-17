#pragma once

#include <utility>

#include "Displays/DisplaySystem.hpp"
#include "Misc/BMPImage.hpp"
#include "Misc/Platform.hpp"
#include "Models/Place.hpp"
#include "config.h"
#include "sindarin-debug.h"

namespace renderer_defs {

// For deep logging when changing this file
static constexpr bool RENDERER_DEBUG = false;
static constexpr bool ON_ERROR_EXIT = false;
static constexpr bool RENDERER_STATS = false;

#define RENDERER_PROFILING 1

#if RENDERER_PROFILING
#define RENDERER_PROFILE_START(v) PROFILE_START(v)
#define RENDERER_PROFILE_END(v) PROFILE_END_LOG_MS(v)
#else
#define RENDERER_PROFILE_START(v) 
#define RENDERER_PROFILE_END(v)
#endif
// Used by the Tokenizer::getFileLocation() method in case of error
static constexpr uint32_t LOCATION_ERROR = 0xFFFFFFFF;

// This is the number of SUB-lines we want to split a line into to be able to render bare
// newlines as less than a full line height of the text. (Deci was chosen as 1/10) So what the
// two lines below mean is that a new line by itself on a line will render as 4/10s of a full
// line height. Theoretically we can do any percentage we want here, but there is a lot of
// testing to do for that!

static constexpr uint16_t NEWLINE_DECI_LINE_HEIGHT = 10;
static constexpr uint16_t ENDPARAGRAPH_DECI_LINE_HEIGHT = 4;

// A Page array must be large enough to accommodate the need to load a complete display page,
// but also able to load a complete paragraph that can precede the page when the back page
// command is issued by the user. By design a paragraph cannot have more than 1500 bytes,
// followed by a NEWLINE, an ENDOFPARAGRAPH, and a null.

static constexpr uint16_t MAX_BYTES_PER_PARAGRAPH = 5003; // 1503;
static constexpr uint16_t MAX_BYTES_PER_DISPLAY_PAGE = 2400;
static constexpr uint16_t MAX_BYTES_TO_RETRIEVE =
    (MAX_BYTES_PER_PARAGRAPH + MAX_BYTES_PER_DISPLAY_PAGE);
static constexpr uint16_t CSTR_POOL_SIZE = 2000;
static constexpr uint16_t TOKENS_SIZE = 500;
static constexpr uint16_t MAX_BYTES_PER_LINE = 200;
static constexpr uint16_t LINES_SIZE = 36;
static constexpr uint16_t PAGE_INDEX_SIZE = 500;

// NOTE:
//
// In the renderer class definitions:
//
// - "Place" as a variable suffix, defines a character index in the file stream
// - "Idx" as a variable suffix, defines an index in one of the arrays maintained by this class

using Idx = uint16_t;

// These are the different tokens so as we parse a buffer
// we more easily know what to do with the text.
enum class TokenId : uint8_t {
    TEXT = 0,     // Usually a word. Can contain NBSP characters
    GTEXT,        // Glued text: Token that must stay on the same line as the next one (TBD)
    NEWLINE,      // Full line vertical space
    ENDPARAGRAPH, // After paragraph vertical space
    HYPHEN,       // ASCII hyphen "-"
    ENDASH,       // Unicode En Dash (U+2013) "–"
    EMDASH,       // Unicode Em Dash (U+2014) "—"
    TAB,          // Tab character
    PICTURE,      // Whole screen picture
    EPICTURE,     // Embedded in page picture (between paragraphs)
    HRULE,        // Horizontal line
    NOTOKEN       // In case a token needs to be hidden
};

#pragma pack(push, 1)

class StreamPlace {
public:
    StreamPlace(uint32_t offset, Idx fileIdx) : offset_(offset), fileIdx_(fileIdx) {}
    StreamPlace() : offset_(0), fileIdx_(0) {}

    inline void setOffset(uint32_t offset) { offset_ = offset; }
    inline void incOffset() { offset_ += 1; }
    inline void decOffset() {
        if (offset_ > 0) {
            offset_ -= 1;
        }
    }
    [[nodiscard]] inline auto getOffset() const -> uint32_t { return offset_; }
    [[nodiscard]] inline auto getFileIdx() const -> Idx { return fileIdx_; }

    [[nodiscard]] inline auto operator==(const StreamPlace &rhs) const -> bool {
        return (offset_ == rhs.offset_) && (fileIdx_ == rhs.fileIdx_);
    }

    [[nodiscard]] auto str() const -> std::string {
        std::string s;
        s = "[offset:";
        s += std::to_string(offset_);
        s.append(", fileIdx:");
        s += std::to_string(fileIdx_);
        s.append("]");
        return s;
    }

private:
    uint32_t offset_;
    Idx fileIdx_;
};

// A token is simply a place in the buffer and a token identifer
// that indicates what type of token it is.
//
// To determine the length of a token, we can simply look at the
// next token in the buffer and subtract the current token's
// place.
//
// Fields ordering is to optimize the space required.
struct Token {
    // This basically gives each token knowledge of where the paragraph
    // start is for where it's located. It's useful for looking backwards
    // while rendering a back page command.
    const char *cStr;
    std::shared_ptr<BMPImage> picture;
    StreamPlace streamPlace;
    uint16_t pixWidth;  // Width in pixels. Used with pictures and text tokens
    uint16_t pixHeight; // Height in pixels. Used with pictures tokens
    uint16_t length;    // Length in bytes
    TokenId tokenId;
    uint8_t spacesAfter;
};

struct LineInfo {
    Idx tokenIdx;
    uint16_t tokenCount;
    uint16_t length;     // String length
    uint16_t deciHeight; // Height in decilines
};
#pragma pack(pop)

using TokensArray = std::array<Token, TOKENS_SIZE>;
using CStrPool = std::array<char, CSTR_POOL_SIZE>;

using Line = std::array<char, MAX_BYTES_PER_LINE>;
using Lines = std::array<LineInfo, LINES_SIZE>;

using PageIndex = std::array<uint32_t, PAGE_INDEX_SIZE>;

} // namespace renderer_defs