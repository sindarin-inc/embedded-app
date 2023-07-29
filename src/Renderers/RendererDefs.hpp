#pragma once

#include <utility>

#include "Displays/DisplaySystem.hpp"
#include "Misc/Platform.hpp"
#include "Models/Place.hpp"
#include "config.h"
#include "sindarin-debug.h"

namespace renderer_defs {

// For deep logging when changing this file
static constexpr bool RENDERER_DEBUG = false;
static constexpr bool RENDERER_CHRONO = false;
static constexpr bool ON_ERROR_EXIT = false;

// Used by the Tokenizer::getFileLocation() method in case of error
static constexpr uint32_t LOCATION_ERROR = 0xFFFFFFFF;

// This is the number of SUB-lines we want to split a line into to be able to render bare
// newlines as less than a full line height of the text. (Deci was chosen as 1/10) So what the
// two lines below mean is that a new line by itself on a line will render as 4/10s of a full
// line height. Theoretically we can do any percent       age we want here, but there is a lot of
// testing to do for that!

static constexpr uint16_t NEWLINE_DECI_LINE_HEIGHT = 10;
static constexpr uint16_t ENDPARAGRAPH_DECI_LINE_HEIGHT = 4;

// A Page array must be large enough to accommodate the need to load a complete display page,
// but also able to load a complete paragraph that can precede the page when the back page
// command is issued by the user. By design a paragraph cannot have more than 1500 bytes,
// followed by a NEWLINE, an ENDOFPARAGRAPH, and a null.

static constexpr uint16_t MAX_BYTES_PER_PARAGRAPH = /* 10003; */ 1503;
static constexpr uint16_t MAX_BYTES_PER_DISPLAY_PAGE = 2400;
static constexpr uint16_t MAX_BYTES_TO_RETRIEVE =
    (MAX_BYTES_PER_PARAGRAPH + MAX_BYTES_PER_DISPLAY_PAGE);
static constexpr uint16_t POOL_BUFFER_SIZE = (MAX_BYTES_PER_PARAGRAPH + MAX_BYTES_PER_DISPLAY_PAGE);
static constexpr uint16_t MAX_TOKEN_COUNT = POOL_BUFFER_SIZE;
static constexpr uint16_t MAX_BYTES_PER_LINE = 200;
static constexpr uint16_t MAX_LINES_PER_PAGE = 200;

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
    TEXT = 0,
    NEWLINE,
    ENDPARAGRAPH,
    SPACE,
    HYPHEN,
    ENDASH,
    EMDASH,
    TAB,
    PICTURE,
    NOTOKEN
};

#pragma pack(push, 1)
struct StreamPlace {
    uint32_t offset;
    uint16_t fileNumber;

    auto operator==(const StreamPlace &rhs) const -> bool {
        return (offset == rhs.offset) && (fileNumber == rhs.fileNumber);
    }

    [[nodiscard]] auto str() const -> std::string {
        std::string s;
        s = "[offset:";
        s += std::to_string(offset);
        s.append(", fileNbr:");
        s += std::to_string(fileNumber);
        s.append("]");
        return s;
    }
};
#pragma pack(pop)

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
    StreamPlace streamPlace;
    Idx prevNewParagraphTokenIdx;
    uint16_t width;
    uint16_t height;
    uint16_t length; // Length in bytes
    TokenId tokenId;
};

struct LineStruct {
    Idx tokenIdx;
    uint16_t tokenCount;
    uint16_t length; // String length
};

using TokensArray = std::array<Token, MAX_TOKEN_COUNT>;
using CStrPool = std::array<char, POOL_BUFFER_SIZE + 1>;

using Line = std::array<char, MAX_BYTES_PER_LINE>;
using Lines = std::array<LineStruct, MAX_LINES_PER_PAGE>;

} // namespace renderer_defs