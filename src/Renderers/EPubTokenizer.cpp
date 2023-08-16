#include "EPubTokenizer.hpp"

// clang-format off

// ToDo: Some cleanup required... there maybe more tags than required here

EPubTokenizer::Tags EPubTokenizer::tags
  = {{"p",           Tag::P}, {"div",               Tag::DIV}, {"span", Tag::SPAN}, {"br",   Tag::BREAK}, {"h1",                 Tag::H1},  
     {"h2",         Tag::H2}, {"h3",                 Tag::H3}, {"h4",     Tag::H4}, {"h5",      Tag::H5}, {"h6",                 Tag::H6}, 
     {"b",           Tag::B}, {"i",                   Tag::I}, {"em",     Tag::EM}, {"body",  Tag::BODY}, {"a",                   Tag::A},
     {"img",       Tag::IMG}, {"image",           Tag::IMAGE}, {"li",     Tag::LI}, {"pre",    Tag::PRE}, {"blockquote", Tag::BLOCKQUOTE},
     {"strong", Tag::STRONG}, {"sub",               Tag::SUB}, {"sup",   Tag::SUP}, {"none",  Tag::NONE}, {"*",                 Tag::ANY}, 
     {"@page",    Tag::PAGE}, {"@font-face",  Tag::FONT_FACE},
    };

// ToDo: Only put the ones that are defined in the Sol Font

EPubTokenizer::Elements EPubTokenizer::elements 
  = {
    {"nbsp",   "\xC2\xA0"},     // no-break space = non-breaking space, U+00A0 ISOnum
    {"iexcl",  "\xC2\xA1"},     // inverted exclamation mark, U+00A1 ISOnum
    {"cent",   "\xC2\xA2"},     // cent sign, U+00A2 ISOnum
    {"pound",  "\xC2\xA3"},     // pound sign, U+00A3 ISOnum
    {"curren", "\xC2\xA4"},     // currency sign, U+00A4 ISOnum
    {"yen",    "\xC2\xA5"},     // yen sign = yuan sign, U+00A5 ISOnum
    {"brvbar", "\xC2\xA6"},     // broken bar = broken vertical bar, U+00A6 ISOnum
    {"sect",   "\xC2\xA7"},     // section sign, U+00A7 ISOnum
    {"uml",    "\xC2\xA8"},     // diaeresis = spacing diaeresis, U+00A8 ISOdia
    {"copy",   "\xC2\xA9"},     // copyright sign, U+00A9 ISOnum
    {"ordf",   "\xC2\xAA"},     // feminine ordinal indicator, U+00AA ISOnum
    {"laquo",  "\xC2\xAB"},     // left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum
    {"not",    "\xC2\xAC"},     // not sign = angled dash, U+00AC ISOnum
    {"shy",    "\xC2\xAD"},     // soft hyphen = discretionary hyphen, U+00AD ISOnum
    {"reg",    "\xC2\xAE"},     // registered sign = registered trade mark sign, U+00AE ISOnum
    {"macr",   "\xC2\xAF"},     // macron = spacing macron = overline = APL overbar, U+00AF ISOdia
    {"deg",    "\xC2\xB0"},     // degree sign, U+00B0 ISOnum
    {"plusmn", "\xC2\xB1"},     // plus-minus sign = plus-or-minus sign, U+00B1 ISOnum
    {"sup2",   "\xC2\xB2"},     // superscript two = superscript digit two = squared, U+00B2 ISOnum
    {"sup3",   "\xC2\xB3"},     // superscript three = superscript digit three = cubed, U+00B3 ISOnum
    {"acute",  "\xC2\xB4"},     // acute accent = spacing acute, U+00B4 ISOdia
    {"micro",  "\xC2\xB5"},     // micro sign, U+00B5 ISOnum
    {"para",   "\xC2\xB6"},     // pilcrow sign = paragraph sign, U+00B6 ISOnum
    {"middot", "\xC2\xB7"},     // middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum
    {"cedil",  "\xC2\xB8"},     // cedilla = spacing cedilla, U+00B8 ISOdia
    {"sup1",   "\xC2\xB9"},     // superscript one = superscript digit one, U+00B9 ISOnum
    {"ordm",   "\xC2\xBA"},     // masculine ordinal indicator, U+00BA ISOnum
    {"raquo",  "\xC2\xBB"},     // right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum
    {"frac14", "\xC2\xBC"},     // vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum
    {"frac12", "\xC2\xBD"},     // vulgar fraction one half = fraction one half, U+00BD ISOnum
    {"frac34", "\xC2\xBE"},     // vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum
    {"iquest", "\xC2\xBF"},     // inverted question mark = turned question mark, U+00BF ISOnum
    {"Agrave", "\xC3\x80"},     // latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1
    {"Aacute", "\xC3\x81"},     // latin capital letter A with acute, U+00C1 ISOlat1
    {"Acirc",  "\xC3\x82"},     // latin capital letter A with circumflex, U+00C2 ISOlat1
    {"Atilde", "\xC3\x83"},     // latin capital letter A with tilde, U+00C3 ISOlat1
    {"Auml",   "\xC3\x84"},     // latin capital letter A with diaeresis, U+00C4 ISOlat1
    {"Aring",  "\xC3\x85"},     // latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1
    {"AElig",  "\xC3\x86"},     // latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1
    {"Ccedil", "\xC3\x87"},     // latin capital letter C with cedilla, U+00C7 ISOlat1
    {"Egrave", "\xC3\x88"},     // latin capital letter E with grave, U+00C8 ISOlat1
    {"Eacute", "\xC3\x89"},     // latin capital letter E with acute, U+00C9 ISOlat1
    {"Ecirc",  "\xC3\x8A"},     // latin capital letter E with circumflex, U+00CA ISOlat1
    {"Euml",   "\xC3\x8B"},     // latin capital letter E with diaeresis, U+00CB ISOlat1
    {"Igrave", "\xC3\x8C"},     // latin capital letter I with grave, U+00CC ISOlat1
    {"Iacute", "\xC3\x8D"},     // latin capital letter I with acute, U+00CD ISOlat1
    {"Icirc",  "\xC3\x8E"},     // latin capital letter I with circumflex, U+00CE ISOlat1
    {"Iuml",   "\xC3\x8F"},     // latin capital letter I with diaeresis, U+00CF ISOlat1
    {"ETH",    "\xC3\x90"},     // latin capital letter ETH, U+00D0 ISOlat1
    {"Ntilde", "\xC3\x91"},     // latin capital letter N with tilde, U+00D1 ISOlat1
    {"Ograve", "\xC3\x92"},     // latin capital letter O with grave, U+00D2 ISOlat1
    {"Oacute", "\xC3\x93"},     // latin capital letter O with acute, U+00D3 ISOlat1
    {"Ocirc",  "\xC3\x94"},     // latin capital letter O with circumflex, U+00D4 ISOlat1
    {"Otilde", "\xC3\x95"},     // latin capital letter O with tilde, U+00D5 ISOlat1
    {"Ouml",   "\xC3\x96"},     // latin capital letter O with diaeresis, U+00D6 ISOlat1
    {"times",  "\xC3\x97"},     // multiplication sign, U+00D7 ISOnum
    {"Oslash", "\xC3\x98"},     // latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1
    {"Ugrave", "\xC3\x99"},     // latin capital letter U with grave, U+00D9 ISOlat1
    {"Uacute", "\xC3\x9A"},     // latin capital letter U with acute, U+00DA ISOlat1
    {"Ucirc",  "\xC3\x9B"},     // latin capital letter U with circumflex, U+00DB ISOlat1
    {"Uuml",   "\xC3\x9C"},     // latin capital letter U with diaeresis, U+00DC ISOlat1
    {"Yacute", "\xC3\x9D"},     // latin capital letter Y with acute, U+00DD ISOlat1
    {"THORN",  "\xC3\x9E"},     // latin capital letter THORN, U+00DE ISOlat1
    {"szlig",  "\xC3\x9F"},     // latin small letter sharp s = ess-zed, U+00DF ISOlat1
    {"agrave", "\xC3\xA0"},     // latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1
    {"aacute", "\xC3\xA1"},     // latin small letter a with acute, U+00E1 ISOlat1
    {"acirc",  "\xC3\xA2"},     // latin small letter a with circumflex, U+00E2 ISOlat1
    {"atilde", "\xC3\xA3"},     // latin small letter a with tilde, U+00E3 ISOlat1
    {"auml",   "\xC3\xA4"},     // latin small letter a with diaeresis, U+00E4 ISOlat1
    {"aring",  "\xC3\xA5"},     // latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1
    {"aelig",  "\xC3\xA6"},     // latin small letter ae = latin small ligature ae, U+00E6 ISOlat1
    {"ccedil", "\xC3\xA7"},     // latin small letter c with cedilla, U+00E7 ISOlat1
    {"egrave", "\xC3\xA8"},     // latin small letter e with grave, U+00E8 ISOlat1
    {"eacute", "\xC3\xA9"},     // latin small letter e with acute, U+00E9 ISOlat1
    {"ecirc",  "\xC3\xAA"},     // latin small letter e with circumflex, U+00EA ISOlat1
    {"euml",   "\xC3\xAB"},     // latin small letter e with diaeresis, U+00EB ISOlat1
    {"igrave", "\xC3\xAC"},     // latin small letter i with grave, U+00EC ISOlat1
    {"iacute", "\xC3\xAD"},     // latin small letter i with acute, U+00ED ISOlat1
    {"icirc",  "\xC3\xAE"},     // latin small letter i with circumflex, U+00EE ISOlat1
    {"iuml",   "\xC3\xAF"},     // latin small letter i with diaeresis, U+00EF ISOlat1
    {"eth",    "\xC3\xB0"},     // latin small letter eth, U+00F0 ISOlat1
    {"ntilde", "\xC3\xB1"},     // latin small letter n with tilde, U+00F1 ISOlat1
    {"ograve", "\xC3\xB2"},     // latin small letter o with grave, U+00F2 ISOlat1
    {"oacute", "\xC3\xB3"},     // latin small letter o with acute, U+00F3 ISOlat1
    {"ocirc",  "\xC3\xB4"},     // latin small letter o with circumflex, U+00F4 ISOlat1
    {"otilde", "\xC3\xB5"},     // latin small letter o with tilde, U+00F5 ISOlat1
    {"ouml",   "\xC3\xB6"},     // latin small letter o with diaeresis, U+00F6 ISOlat1
    {"divide", "\xC3\xB7"},     // division sign, U+00F7 ISOnum
    {"oslash", "\xC3\xB8"},     // latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1
    {"ugrave", "\xC3\xB9"},     // latin small letter u with grave, U+00F9 ISOlat1
    {"uacute", "\xC3\xBA"},     // latin small letter u with acute, U+00FA ISOlat1
    {"ucirc",  "\xC3\xBB"},     // latin small letter u with circumflex, U+00FB ISOlat1
    {"uuml",   "\xC3\xBC"},     // latin small letter u with diaeresis, U+00FC ISOlat1
    {"yacute", "\xC3\xBD"},     // latin small letter y with acute, U+00FD ISOlat1
    {"thorn",  "\xC3\xBE"},     // latin small letter thorn, U+00FE ISOlat1
    {"yuml",   "\xC3\xBF"},     // latin small letter y with diaeresis, U+00FF ISOlat1
    {"quot",   "\x22"},         // quotation mark, U+0022 ISOnum
    {"amp",    "\x26"},         // ampersand, U+0026 ISOnum
    {"lt",     "\x3C"},         // less-than sign, U+003C ISOnum
    {"gt",     "\x3E"},         // greater-than sign, U+003E ISOnum
    {"apos",   "\x27"},         // apostrophe = APL quote, U+0027 ISOnum
    {"OElig",  "\xC5\x92"},     // latin capital ligature OE, U+0152 ISOlat2
    {"oelig",  "\xC5\x93"},     // latin small ligature oe, U+0153 ISOlat2
    {"Scaron", "\xC5\xA0"},     // latin capital letter S with caron, U+0160 ISOlat2
    {"scaron", "\xC5\xA1"},     // latin small letter s with caron, U+0161 ISOlat2
    {"Yuml",   "\xC5\xB8"},     // latin capital letter Y with diaeresis, U+0178 ISOlat2
    {"circ",   "\xCB\x86"},     // modifier letter circumflex accent, U+02C6 ISOpub
    {"tilde",  "\xCB\x9C"},     // small tilde, U+02DC ISOdia
    {"ensp",   "\xE2\x80\x82"}, // en space, U+2002 ISOpub
    {"emsp",   "\xE2\x80\x83"}, // em space, U+2003 ISOpub
    {"thinsp", "\xE2\x80\x89"}, // thin space, U+2009 ISOpub
    {"zwnj",   "\xE2\x80\x8C"}, // zero width non-joiner, U+200C NEW RFC 2070
    {"zwj",    "\xE2\x80\x8D"}, // zero width joiner, U+200D NEW RFC 2070
    {"lrm",    "\xE2\x80\x8E"}, // left-to-right mark, U+200E NEW RFC 2070
    {"rlm",    "\xE2\x80\x8F"}, // right-to-left mark, U+200F NEW RFC 2070
    {"ndash",  "\xE2\x80\x93"}, // en dash, U+2013 ISOpub
    {"mdash",  "\xE2\x80\x94"}, // em dash, U+2014 ISOpub
    {"lsquo",  "\xE2\x80\x98"}, // left single quotation mark, U+2018 ISOnum
    {"rsquo",  "\xE2\x80\x99"}, // right single quotation mark, U+2019 ISOnum
    {"sbquo",  "\xE2\x80\x9A"}, // single low-9 quotation mark, U+201A NEW
    {"ldquo",  "\xE2\x80\x9C"}, // left double quotation mark,  U+201C ISOnum
    {"rdquo",  "\xE2\x80\x9D"}, // right double quotation mark, U+201D ISOnum
    {"bdquo",  "\xE2\x80\x9E"}, // double low-9 quotation mark, U+201E NEW
    {"dagger", "\xE2\x80\xA0"}, // dagger, U+2020 ISOpub
    {"Dagger", "\xE2\x80\xA1"}, // double dagger, U+2021 ISOpub
    {"permil", "\xE2\x80\xB0"}, // per mille sign, U+2030 ISOtech
    {"lsaquo", "\xE2\x80\xB9"}, // single left-pointing angle quotation mark, U+2039 ISO proposed
    {"rsaquo", "\xE2\x80\xBA"}, // single right-pointing angle quotation mark, U+203A ISO proposed
    {"euro",   "\xE2\x80\xAC"}, // euro sign, U+20AC NEW
};

// clang-format on

auto EPubTokenizer::getCharEntity(const char *str, const char **newStrLocation) const -> const
    char * {
    const char *start = str;
    unsigned int len = 0;
    while ((len++ < 10) && (*str != '\0') && (*str != ';')) {
        str++;
    }
    if (*str == ';') {
        auto search = elements.at(std::string(start, str - start));
        str++;
        if (search != nullptr) {
            *newStrLocation = str;
            return search;
        }
    }

    return "";
}

//
// This loads from the stream character by character and populates
// the token buffer based on the TOKEN enum.
// After this is done, then we can do the heavy lifting of walking
// through the tokens, checking size, and rendering the text on
// a line by line basis.
//
// We track backwards because we need to double the amount read in a backwards
// read (2 pages instead of 1)
//
// Computes the number of word in the page.
//
// Returns the computed streamPlace where the page start really.
//
auto EPubTokenizer::loadAndTokenizePage(StreamPlace fromPlace, bool backwards, StreamPlace *toPlace)
    -> StreamPlace {

    // Not pretty, but limits the number of parameters to pass to the interpret() method
    fromPlace_ = fromPlace;
    toPlace_ = toPlace;
    backwards_ = backwards;
    prevNewParagraphIdx_ = 0;
    lastStreamRelativeOffset_ = fromPlace.getOffset();

    // log_w("Opening file at streamPlace %s", fromPlace.str().c_str());

    const EPubOpf::SpineItem &spineItem = stream_->getSpine(fromPlace.getFileNumber());
    pugi::xml_document &doc = stream_->getXHTMLFile(spineItem.item->href);

    tokens_.clear();
    pager_.clear();

    if (doc.child("html")) {

        started_ = false;
        pageCompleted_ = false;

        pugi::xml_node node = doc.child("html").child("body").first_child();

        if (node) {
            do {
                if (!interpret(node)) {
                    break;
                }
                node = node.next_sibling();
            } while (node);
        }

        if (!node) {
            log_e("-------------- End of file detected --------------");
            Idx maxIdx = stream_->getSpineCount();
            if ((fromPlace.getFileNumber() + 1) < maxIdx) {
                pager_.setNextPagePlace({0, fromPlace.getFileNumber() + 1});
            } else {
                pager_.endOfFileReached();
            }
        }
    } else {
        log_e("File is of the wrong format");
        if constexpr (ON_ERROR_EXIT) {
            exit(1);
        }
    }

    return fromPlace;
}

auto EPubTokenizer::interpret(pugi::xml_node node) -> bool {

    uint32_t offset;

    if (node.type() == pugi::node_element) {
        // log_w("Interpreting %s", node.name());
        offset = stream_->getOffset(node.name());
        if (backwards_ && (offset >= toPlace_->getOffset())) {
            return false;
        }
        for (pugi::xml_node n : node.children()) {
            if (!interpret(n)) {
                return false;
            }
        }
        // if constexpr (RENDERER_DEBUG) {
        //     if (backwards_) {
        //         log_w("Backwards processing at end of node %s: lastStreamRelativeOffset_:%"
        //         PRIu32
        //               ", toPlace offset:%" PRIu32,
        //               node.name(), lastStreamRelativeOffset_, toPlace_->getOffset());
        //     }
        // }
        lastStreamRelativeOffset_ += 1;
        pageCompleted_ =
            pageCompleted_ || (backwards_ && (lastStreamRelativeOffset_ >= toPlace_->getOffset()));
        if (!pageCompleted_) { //(!backwards_ || (lastStreamRelativeOffset_ <
                               //toPlace_->getOffset())) {
            Tags::iterator tagIt = tags.find(node.name());
            if (tagIt != tags.end() && started_) {
                const Tag &tag = tagIt->second;

                switch (tag) {
                case Tag::P:
                case Tag::H1:
                case Tag::H2:
                case Tag::H3:
                case Tag::H4:
                case Tag::H5:
                case Tag::H6:
                    // put a END_PARAGRAPH
                    // if constexpr (RENDERER_DEBUG) {
                    //     if (backwards_) {
                    //         log_w("Backwards processing at end of node %s: "
                    //               "lastStreamRelativeOffset_:%" PRIu32 ", toPlace offset:%"
                    //               PRIu32, node.name(), lastStreamRelativeOffset_,
                    //               toPlace_->getOffset());
                    //     }
                    // }
                    if (addTokenToPage(
                            {.cStr = "\n",
                             .streamPlace = {lastStreamRelativeOffset_, fromPlace_.getFileNumber()},
                             .prevNewParagraphTokenIdx = prevNewParagraphIdx_,
                             .width = 0,
                             .height = 0,
                             .length = 1,
                             .tokenId = TokenId::ENDPARAGRAPH},
                            backwards_)) {
                        return false;
                    }
                    break;

                case Tag::BREAK:
                    // put a NEWLINE
                    if (addTokenToPage(
                            {.cStr = "\n",
                             .streamPlace = {lastStreamRelativeOffset_, fromPlace_.getFileNumber()},
                             .prevNewParagraphTokenIdx = prevNewParagraphIdx_,
                             .width = 0,
                             .height = 0,
                             .length = 1,
                             .tokenId = TokenId::NEWLINE},
                            backwards_)) {
                        return false;
                    }
                    break;

                default:
                    break;
                }
            }
        } else {
            pager_.completeLastLine();
            return false;
        }

    } else if (node.type() == pugi::node_pcdata) {
        // log_w("Interpreting content %s", node.value());

        const pugi::char_t *str = node.value();
        uint32_t offset = stream_->getOffset(str);
        uint32_t len = strlen(str);

        if (!started_) {
            if ((offset + len) >= fromPlace_.getOffset()) {
                started_ = true;
                uint32_t bypass =
                    (fromPlace_.getOffset() > offset) ? fromPlace_.getOffset() - offset : 0;
                return addStrToPage(str + bypass, len - bypass);
            }
        } else {
            if (backwards_) {
                if (offset >= toPlace_->getOffset()) {
                    return false;
                }
                if ((offset + len) > toPlace_->getOffset()) {
                    len = toPlace_->getOffset() - offset;
                }
            }

            return addStrToPage(str, len);
        }
    }

    return true;
}

auto EPubTokenizer::addStrToPage(const pugi::char_t *str, uint16_t len) -> bool {

    uint32_t streamRelativeOffset = stream_->getOffset(str);

    Token newToken;

    // Keep it DRY

    uint8_t curChar = *str++;
    len--;

    auto nextByte = [&curChar, &str, &len, &streamRelativeOffset]() -> void {
        if (len == 0) {
            curChar = 0;
        } else {
            curChar = *str++;
            len--;
            streamRelativeOffset++;
        }
    };

    // If we go backwards, get rid of the first bytes that could be the end of a line/paragraph
    // if (backwards_ && (fromPlace.getOffset() != 0)) {
    //     while ((curChar != 0) && (curChar != '\n')) {
    //         nextByte();
    //     }
    //     if (curChar == '\n') {
    //         nextByte();
    //     }
    // }

    // if constexpr (RENDERER_DEBUG) {
    //     log_d("First Character: %c (%02" PRIX8 "H)",
    //           (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);
    // }

    while ((!pageCompleted_) && (curChar != 0) && (!tokens_.isFull())) {

        newToken = {.cStr = nullptr,
                    .streamPlace = {streamRelativeOffset, fromPlace_.getFileNumber()},
                    .prevNewParagraphTokenIdx = prevNewParagraphIdx_,
                    .width = 0,
                    .height = 0,
                    .length = 0,
                    .tokenId = TokenId::NOTOKEN};

        if (backwards_ && !tokens_.isEmpty() &&
            (newToken.streamPlace.getOffset() >= toPlace_->getOffset())) {
            pager_.completeLastLine();
            pageCompleted_ = true;
            break;
        }

        // if constexpr (RENDERER_DEBUG) {
        //     log_d("Set the stream place to %s",
        //           addOffset(fromPlace, streamRelativeOffset).str().c_str());
        //     log_d("Current Character: %c (%02" PRIX8 "H)",
        //           (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);
        // }

        switch (curChar) {
        case '\n':
            // No two or more lines with only a \n are allowed.
            newToken.tokenId = TokenId::NEWLINE;
            pageCompleted_ = addTokenToPage(newToken, backwards_, "\n", 1);
            if (!pageCompleted_) {
                nextByte();
                if (curChar == '\n') {
                    prevNewParagraphIdx_ = tokens_.getCount();
                    newToken.tokenId = TokenId::ENDPARAGRAPH;
                    pageCompleted_ = addTokenToPage(newToken, backwards_, "\n", 1);
                    nextByte();
                    while (curChar == '\n') {
                        nextByte();
                    }
                }
            }

            break;

        case ' ':
            newToken.tokenId = TokenId::SPACE;
            tokens_.setSpaceSize(newToken);
            pageCompleted_ = addTokenToPage(newToken, backwards_, " ", 1);
            nextByte();
            break;

        case '\t':
            newToken.tokenId = TokenId::TAB;
            pageCompleted_ = addTokenToPage(newToken, backwards_, "\t", 1);
            nextByte();
            break;

        case '-':
            nextByte();
            if (curChar != '-') {
                newToken.tokenId = TokenId::HYPHEN;
                pageCompleted_ = addTokenToPage(newToken, backwards_, "-", 1);
            } else {
                nextByte();
                if (curChar != '-') {
                    newToken.tokenId = TokenId::ENDASH;
                    pageCompleted_ = addTokenToPage(newToken, backwards_, "--", 2);
                } else {
                    nextByte();
                    newToken.tokenId = TokenId::EMDASH;
                    pageCompleted_ = addTokenToPage(newToken, backwards_, "---", 3);
                }
            }
            break;

        // All other characters are considered part of a text token,
        default:
            newToken.tokenId = TokenId::TEXT;
            uint16_t textCharCount = 0;

            do {
                if (curChar == '&') {
                    const char *newStrLocation;
                    const char *trans = getCharEntity(str, &newStrLocation);
                    if (*trans != '\0') {
                        len -= (newStrLocation - str);
                        streamRelativeOffset += (newStrLocation - str);
                        while (*trans && !tokens_.cStrIsFull()) {
                            tokens_.addByte(newToken, *trans++);
                        }
                        textCharCount += 1;
                        str = newStrLocation;
                        nextByte();
                    } else {
                        if (!tokens_.cStrIsFull()) {
                            tokens_.addByte(newToken, curChar);
                            textCharCount += 1;
                        } else {
                            break;
                        }
                        nextByte();
                    }
                } else {
                    if ((curChar <= 0x7FU) || (curChar >= 0xC0U)) {
                        textCharCount += 1;
                    }
                    if (!tokens_.cStrIsFull()) {
                        tokens_.addByte(newToken, curChar);
                    } else {
                        break;
                    }
                    nextByte();
                }
            } while ((curChar > ' ') && (curChar != '-') &&
                     ((textCharCount < maxTextTokenCharSize_) || (curChar >= 0x80U)));
            // The last test of the while above is to get a complete UTF8 multi-bytes character
            // if the maximum word size is reached

            pageCompleted_ = addTokenToPage(newToken, backwards_);
            break;
        }
    }

    if (!pageCompleted_ && tokens_.cStrIsFull()) {
        // There is a potential that the last token is not complete (e.g. a TEXT or ENDASH or
        // EMDASH), so forget it
        tokens_.forgetLast();
    }

    if constexpr (RENDERER_DEBUG) {
        log_d("Computed Word Count: %" PRIu16, pager_.getWordCount());
        log_d("Tokens Count: %" PRIu16, tokens_.getCount());
    }

    lastStreamRelativeOffset_ = streamRelativeOffset;

    return !pageCompleted_;
}