#pragma once

#include <utility>

#include "Displays/DisplaySystem.hpp"
#include "EPub/EPubDefs.hpp"
#include "Misc/BMPImage.hpp"
#include "Misc/Platform.hpp"
#include "Models/Place.hpp"
#include "config.h"
#include "sindarin-debug.h"

namespace renderer_defs {

// The following defines are helpers for code development
// That is specific to the renderer

#define REND_DEBUGGING 0     // Show debugging messages where appropriate
#define REND_EXIT_ON_ERROR 0 // Simulator Only!!
#define REND_PROFILING 0     // Speed measurements
#define REND_STATISTICS 0    // Stats on Tokens size and Lines size

#if !SIMULATOR
#undef REND_EXIT_ON_ERROR // To be sure there will be no exit() call on the Sol device
#endif

#if REND_EXIT_ON_ERROR
static constexpr bool REND_ON_ERROR_EXIT = true;
#define REND_EXIT_SIM exit(1)
#define REND_ELOG(f, ...)                                                                          \
    {                                                                                              \
        log_e(f, ##__VA_ARGS__);                                                                   \
        exit(1);                                                                                   \
    }
#else
static constexpr bool REND_ON_ERROR_EXIT = false;
#define REND_EXIT_SIM
#endif

#if REND_DEBUGGING
static constexpr bool REND_DEBUG = true;
#define REND_WLOG(f, ...) log_w(f, ##__VA_ARGS__);
#ifndef REND_ELOG
#define REND_ELOG(f, ...) log_e(f, ##__VA_ARGS__);
#endif
#else
static constexpr bool REND_DEBUG = false;
#define REND_WLOG(...)
#ifndef REND_ELOG
#define REND_ELOG(...)
#endif
#endif

#if REND_PROFILING
#define REND_PROFILE_START(v) PROFILE_START(v)
#define REND_PROFILE_END(v) PROFILE_END_LOG_MS(v)
#else
#define REND_PROFILE_START(v)
#define REND_PROFILE_END(v)
#endif

#if REND_STATISTICS
static constexpr bool REND_STATS = true;
#else
static constexpr bool REND_STATS = false;
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
static constexpr uint16_t HRULE_DECI_LINE_HEIGHT = 4;

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

static constexpr uint32_t END_OFFSET = 999999999U;
// NOTE:
//
// In the renderer class definitions:
//
// - "Place" as a variable suffix, defines a character index in the file stream
// - "Idx" as a variable suffix, defines an index in one of the arrays maintained by this class

using Idx = int;

// These are the different tokens so as we parse a buffer, we more easily know what to do with the
// text. Bits are assigned to each such that they can be combined in a mask for comparisons.
enum class TokenId : uint16_t {
    TEXT = 1 << 0,    // Usually a word. Can contain &NBSP; and spaces characters for formatting
    ITEXT = 1 << 1,   // Indent *next* lines using this token width
    HTEXT = 1 << 2,   // Start of a <H1>..<h6> header tag
    NTEXT = 1 << 3,   // Text that is *NOT* in the pool
    NEWLINE = 1 << 4, // Full line vertical space. Break lines
    ENDPARAGRAPH = 1 << 5, // After paragraph vertical space
    HYPHEN = 1 << 6,       // ASCII hyphen "-"
    ENDASH = 1 << 7,       // Unicode En Dash (U+2013) "–"
    EMDASH = 1 << 8,       // Unicode Em Dash (U+2014) "—"
    PICTURE = 1 << 9,      // Whole screen picture
    EPICTURE =
        1 << 10,      // Embedded in page picture (between paragraphs) More than one can be present
    HRULE = 1 << 11,  // Horizontal line
    BTABLE = 1 << 12, // Beginning of a table
    ETABLE = 1 << 13  // End of table
};

#if REND_DEBUGGING
static const char *TokenIdStr[] = {"TEXT",    "ITEXT",        "HTEXT",    "NTEXT",
                                   "NEWLINE", "ENDPARAGRAPH", "HYPHEN",   "ENDASH",
                                   "EMDASH",  "PICTURE",      "EPICTURE", "HRULE"};
#endif

// BitMask of all token ids that are using the pool space
static const uint16_t POOL_TOKENS =
    static_cast<uint16_t>(TokenId::TEXT) | static_cast<uint16_t>(TokenId::ITEXT) |
    static_cast<uint16_t>(TokenId::HTEXT) | static_cast<uint16_t>(TokenId::PICTURE) |
    static_cast<uint16_t>(TokenId::EPICTURE);
#pragma pack(push, 1)

class StreamPlace {
public:
    StreamPlace(uint32_t offset, Idx spineIdx) : offset_(offset), spineIdx_(spineIdx) {}
    StreamPlace() : offset_(0), spineIdx_(0) {}

    inline void setOffset(uint32_t offset) { offset_ = offset; }
    inline void incOffset() { offset_ += 1; }
    inline void decOffset() {
        if (offset_ > 0) {
            offset_ -= 1;
        }
    }
    [[nodiscard]] inline auto getOffset() const -> uint32_t { return offset_; }
    [[nodiscard]] inline auto getSpineIdx() const -> SpineIdx { return spineIdx_; }

    [[nodiscard]] inline auto operator==(const StreamPlace &rhs) const -> bool {
        return (offset_ == rhs.offset_) && (spineIdx_ == rhs.spineIdx_);
    }

    [[nodiscard]] inline auto operator>=(const StreamPlace &rhs) const -> bool {
        return (spineIdx_ > rhs.spineIdx_) ||
               ((spineIdx_ == rhs.spineIdx_) && (offset_ >= rhs.offset_));
    }

    [[nodiscard]] auto str() const -> std::string {
        std::string s;
        s = "[offset:";
        s += std::to_string(offset_);
        s.append(", spineIdx:");
        s += std::to_string(spineIdx_);
        s.append("]");
        return s;
    }

private:
    uint32_t offset_;
    Idx spineIdx_;
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
    const char *cStr{};
    std::shared_ptr<BMPImage> picture;
    StreamPlace streamPlace;
    uint16_t pixWidth{};  // Width in pixels. Used with pictures and text tokens
    uint16_t pixHeight{}; // Height in pixels. Used with pictures tokens
    uint16_t length{};    // Length in bytes
    TokenId tokenId{TokenId::TEXT};
    uint8_t spacesAfter{};
};

struct LineInfo {
    Idx tokenIdx;
    uint16_t tokenCount;
    uint16_t length;     // String length
    uint16_t deciHeight; // Height in decilines
    uint16_t leftIndent; // Line Left Indentation
};
#pragma pack(pop)

using TokensArray = std::array<Token, TOKENS_SIZE>;
using CStrPool = std::array<char, CSTR_POOL_SIZE>;

using Line = std::array<char, MAX_BYTES_PER_LINE>;
using Lines = std::array<LineInfo, LINES_SIZE>;

using PageIndex = std::array<uint32_t, PAGE_INDEX_SIZE>;

} // namespace renderer_defs
