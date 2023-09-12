#include "EPubTokenizer.hpp"

// clang-format off

// ToDo: Some cleanup required... there maybe more tags than required here

EPubTokenizer::Tags EPubTokenizer::tags
  = {{"p",             Tag::P}, {"div",               Tag::DIV}, {"span",            Tag::SPAN}, 
     {"br",        Tag::BREAK}, {"h1",                 Tag::H1}, {"h2",                Tag::H2}, 
     {"h3",           Tag::H3}, {"h4",                 Tag::H4}, {"h5",                Tag::H5}, 
     {"h6",           Tag::H6}, {"b",                   Tag::B}, {"i",                  Tag::I},
     {"em",           Tag::EM}, {"a",                   Tag::A}, {"img",              Tag::IMG}, 
     {"picture", Tag::PICTURE}, {"source",         Tag::SOURCE}, {"li",                Tag::LI}, 
     {"pre",         Tag::PRE}, {"blockquote", Tag::BLOCKQUOTE}, {"strong",        Tag::STRONG}, 
     {"sub",         Tag::SUB}, {"sup",               Tag::SUP}, {"none",            Tag::NONE}, 
     {"*",           Tag::ANY}, {"@page",            Tag::PAGE}, {"@font-face", Tag::FONT_FACE},
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
    {"quot",   R"(")"},         // quotation mark, U+0022 ISOnum
    {"amp",    R"(&)"},         // ampersand, U+0026 ISOnum
    {"lt",     R"(<)"},         // less-than sign, U+003C ISOnum
    {"gt",     R"(>)"},         // greater-than sign, U+003E ISOnum
    {"apos",   R"(')"},         // apostrophe = APL quote, U+0027 ISOnum
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

EPubTokenizer::SuperscriptNumbers EPubTokenizer::superscriptNumbers
{ 
    "\xE2\x81\xB0",  // 0
    "\xC2\xB9",      // 1
    "\xC2\xB2",      // 2
    "\xC2\xB3",      // 3
    "\xE2\x81\xB4",  // 4
    "\xE2\x81\xB5",  // 5
    "\xE2\x81\xB6",  // 6
    "\xE2\x81\xB7",  // 7
    "\xE2\x81\xB8",  // 8
    "\xE2\x81\xB9",  // 9
};

// clang-format on

auto EPubTokenizer::computeStartingOffset(StreamPlace streamPlace) -> uint32_t {
    pugi::xml_document &doc = loadXHTMLAt(streamPlace.getFileIdx());

    struct Walker : pugi::xml_tree_walker {
        virtual bool for_each(pugi::xml_node &node) {
            if ((node.type() == pugi::xml_node_type::node_pcdata) &&
                !EPubTokenizer::isSpaces(node.value())) {
                firstNode = node;
                return false;
            }
            return true;
        }

        pugi::xml_node firstNode;
    } walker;

    doc.child("html").child("body").traverse(walker);
    return walker.firstNode ? walker.firstNode.offset_debug() : 0;
}

// Checks if a node is empty (i.e. there is no displayable character and no picture node in it),
// return true if so.
auto EPubTokenizer::isEmpty(pugi::xml_node node) -> bool {

    struct Walker : pugi::xml_tree_walker {
        auto for_each(pugi::xml_node &node) -> bool override {
            if (node.type() == pugi::xml_node_type::node_pcdata) {
                if (!EPubTokenizer::isSpaces(node.value())) {
                    empty = false;
                    return false;
                }
            } else if ((node.type() == pugi::xml_node_type::node_element) &&
                       (strcmp(node.name(), "picture") == 0)) {
                empty = false;
                return false;
            }
            return true;
        }

        bool empty;
    } walker;

    walker.empty = true;
    node.traverse(walker);

    return walker.empty;
}

// goBackward() is used in the context of backwards page displacement. For an EPub
// document, it is rather complex. If the current place is at the first displayable character
// of an XHTML file, we have to move to the preceding file, at the beginning of it.
auto EPubTokenizer::goBackward(StreamPlace streamPlace, uint32_t offsetToSubtract) -> StreamPlace {
    uint32_t fileStartingOffset = computeStartingOffset(streamPlace);

    if (streamPlace.getOffset() <= fileStartingOffset) {
        if (streamPlace.getFileIdx() > 0) {
            return {0, static_cast<uint16_t>(streamPlace.getFileIdx() - 1)};
        } else {
            return streamPlace; // don't move
        }
    } else {
        return {0, streamPlace.getFileIdx()};
    }
}

// An entity as been detected, find it in the table to translate it to the right UTF8 character.
// It has the form &xxx; in text.
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

auto EPubTokenizer::loadXHTMLAt(Idx fileIdx) -> pugi::xml_document & {
    const EPubOpf::SpineItem &spineItem = stream_->getSpine(fileIdx);
    return stream_->getXHTMLFile(spineItem.item->href);
}

// Find the next offset where the first character/picture will come from for the next page. This is
// required to synchronize a backward search: The last location of a page could be at a different
// location as the first location of next page because of xml in-between elements
auto EPubTokenizer::forwardToNextPageOffset(pugi::xml_node node) const -> uint32_t {

    struct Walker : pugi::xml_tree_walker {
        virtual bool for_each(pugi::xml_node &node) {
            if (started) {
                if (node.type() == pugi::xml_node_type::node_pcdata) {
                    if (!EPubTokenizer::isSpaces(node.value())) {
                        offset = theStream->getOffset(node.value());
                        return false;
                    }
                }
            } else {
                if (node.type() == pugi::xml_node_type::node_pcdata) {
                    if (!EPubTokenizer::isSpaces(node.value())) {
                        uint32_t location = theStream->getOffset(node.value());
                        uint32_t length = strlen(node.value());
                        if ((location + length) >= offset) {
                            started = true;
                            offset = (location >= offset) ? location : (offset + 1);
                            return false;
                        }
                    }
                } else if (node.type() == pugi::xml_node_type::node_element) {
                    uint32_t location = theStream->getOffset(node.name());
                    // log_w("Node: %s, location: %" PRIu32, node.name(), location);
                    if (location >= offset) {
                        started = true;
                    }
                }
            }
            return true;
        }

        bool started;
        uint32_t offset;
        std::shared_ptr<EPubFile> theStream;
    } walker;

    walker.started = false;
    walker.offset = fromPlace_.getOffset();
    walker.theStream = stream_;

    node.parent().traverse(walker);

    uint32_t theOffset = walker.started ? walker.offset : 9999999;

    if constexpr (RENDERER_DEBUG) {
        log_w("Computed offset: %" PRIu32 ", search from offset: %" PRIu32, theOffset,
              fromPlace_.getOffset());
    }

    return theOffset;
}

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
    -> std::pair<StreamPlace, StreamPlace> {

    // Not pretty, but limits the number of parameters to pass to the interpret() method
    fromPlace_ = fromPlace;

    if constexpr (RENDERER_DEBUG) {
        log_w("Opening file at streamPlace %s, backwards: %s%s", fromPlace.str().c_str(),
              (backwards ? "YES, Up to: " : "NO"),
              (toPlace == nullptr ? "" : toPlace->str().c_str()));
    }

    pugi::xml_document &doc = loadXHTMLAt(fromPlace.getFileIdx());

    pugi::xml_node node = doc.child("html").child("body").first_child();

    if (node) {
        do {
            lastStreamRelativeOffset_ = fromPlace_.getOffset();
            skipNextParagraph_ = false;
            lastToken_ = nullptr;

            started_ = false;
            pageCompleted_ = false;

            tokens_.clear();
            pager_.clear();

            if (node) {
                do {
                    if (!interpret(node)) {
                        break;
                    }
                    node = node.next_sibling();
                } while (node);
            }

            if (!node) {
                // log_e("-------------- End of file detected --------------");
                Idx maxIdx = stream_->getSpineCount();
                if ((fromPlace.getFileIdx() + 1) < maxIdx) {
                    pager_.setNextPagePlace({0, static_cast<uint16_t>(fromPlace.getFileIdx() + 1)});
                } else {
                    pager_.setEndOfFile();
                }
                pager_.completeLastLine();
                log_w("End of file reached");
                break;
            }

            pager_.completeLastLine();
            fromPlace_ = pager_.getNextPageStreamPlace();

        } while (backwards && (forwardToNextPageOffset(node) < toPlace->getOffset()));
    } else {
        log_e("File is of the wrong format");
        if constexpr (ON_ERROR_EXIT) {
            exit(1);
        }
    }

    if constexpr (RENDERER_DEBUG) {
        log_w("End of job: Page start: %s, next page at: %s",
              pager_.getStreamPlaceOfFirstLine().str().c_str(),
              pager_.getNextPageStreamPlace().str().c_str());
    }

    return {pager_.getStreamPlaceOfFirstLine(), pager_.getNextPageStreamPlace()};
}

auto EPubTokenizer::getStreamPlaceFromTxtOffset(uint32_t txtOffset) const -> StreamPlace {
    if (txtOffset > 0) {
        for (const auto &tocEntry : stream_->getToc()) {
            if ((txtOffset >= tocEntry.txtOffset) && (txtOffset < tocEntry.endTxtOffset)) {
                uint16_t spineIdx = stream_->getSpineIdx(tocEntry.href);
                uint32_t charOffset = txtOffset - tocEntry.txtOffset;
                if (charOffset == 0) {
                    return {0, spineIdx};
                } else {
                    uint32_t offset =
                        stream_->findFileOffsetAtCharOffset(tocEntry.href, charOffset);
                    return {offset, spineIdx};
                }
                break;
            }
        }
    }

    log_w("txtOffset not found!");
    return {0, 0};
}

auto EPubTokenizer::toStreamPlace(const std::shared_ptr<Place> place) const -> StreamPlace {
    if (place->isFor(DocType::EPUB)) {
        std::shared_ptr<EPubPlace> ePubPlace = std::dynamic_pointer_cast<EPubPlace>(place);
        uint16_t spineIdx = stream_->getSpineIdx(ePubPlace->getHRef());
        return {place->getOffset(), spineIdx};
    } else if (place->isFor(DocType::TXT)) {
        auto pl = getStreamPlaceFromTxtOffset(place->getOffset());
        log_w("Computed place: %s", pl.str().c_str());
        return pl;
    } else {
        log_e("Wrong Place Type: %" PRIu8, static_cast<uint8_t>(place->getDocType()));
        if constexpr (ON_ERROR_EXIT) {
            exit(1);
        }
    }

    return {};
}

void EPubTokenizer::processPictureTag(pugi::xml_node node) {

    pugi::xml_node source = node.find_child_by_attribute("source", "class", "dithered");

    if (source) {
        // log_w("Found dithered picture...");
        Token &pictureToken = tokens_.getNextAvailable();
        pictureToken = {.cStr = nullptr,
                        .streamPlace = {lastStreamRelativeOffset_, fromPlace_.getFileIdx()},
                        .width = 0,
                        //.height = 0,
                        .length = 0,
                        .tokenId = TokenId::PICTURE,
                        .spacesAfter = 0};
        tokens_.addStr(pictureToken, stream_->getFullPath(source.attribute("srcset").value()));

        auto [buffer, len] = stream_->getFile(source.attribute("srcset").value());
        if (buffer != nullptr) {
            std::shared_ptr<BMPImage> image = std::make_shared<BMPImage>();
            image.get()->readFromBytes(buffer.get(), len);
            pager_.setPicture(image);
        }
        pageCompleted_ = addTokenToPage(pictureToken);
        lastToken_ = &pictureToken;

        // log_w("Page completed? : %s", pageCompleted_ ? "Yes" : "no");
        // } else {
        //     log_w("Dithered picture not found...");
    }
}

void EPubTokenizer::processSuperscript(pugi::xml_node node) {
    const pugi::char_t *text = node.value();
    lastStreamRelativeOffset_ = node.offset_debug(); // stream_->getOffset(text);
    Token &newToken = tokens_.getNextAvailable();
    newToken = {.cStr = nullptr,
                .streamPlace = {lastStreamRelativeOffset_, fromPlace_.getFileIdx()},
                .width = 0,
                //.height = 0,
                .length = 0,
                .tokenId = TokenId::TEXT,
                .spacesAfter = 0};
    while (*text) {
        if ((*text >= '0') && (*text <= '9')) {
            const char *script = superscriptNumbers[*text - '0'];
            while (*script) {
                tokens_.addByte(newToken, *script++);
            }
        }
        text++;
        lastStreamRelativeOffset_++;
    }
    tokens_.endOfCStr();
    if (newToken.length > 0) {
        pageCompleted_ = addTokenToPage(newToken);
        lastToken_ = &newToken;
    }
}

auto EPubTokenizer::interpret(pugi::xml_node node) -> bool {

    uint32_t offset;

    if (node.type() == pugi::node_element) {
        offset = node.offset_debug(); // stream_->getOffset(node.name());
        lastStreamRelativeOffset_ = offset;
        // log_w("Interpreting node element %s at offset %" PRIu32, node.name(), offset);

        if (!started_ && (offset >= fromPlace_.getOffset())) {
            started_ = true;
        }

        auto tagIt = tags.find(node.name());

        // if (tagIt != tags.end()) {
        //     log_e("Tag: %d (%s) at offset %" PRIu32, (int)tag, tagIt->first.c_str(), offset);
        // }

        if (tagIt != tags.end()) {
            if (tagIt->second == Tag::PICTURE) {
                if (started_) {
                    // log_w("Picture found...");
                    processPictureTag(node);
                }
                skipNextParagraph_ = true;
                return true;
            } else if (tagIt->second == Tag::SUP) {
                if (started_) {
                    processSuperscript(node.first_child());
                }
                return !pageCompleted_;
            } else if ((tagIt->second == Tag::P) && isEmpty(node)) {
                skipNextParagraph_ = false;
                return true;
            }
        }
        if (skipNextParagraph_) {
            skipNextParagraph_ = false;
            return true;
        } else {
            for (pugi::xml_node n : node.children()) {
                if (!interpret(n)) {
                    return false;
                }
            }

            lastStreamRelativeOffset_ += 1;

            if (!pageCompleted_) {
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
                        if (!tokens_.isFull() && !skipNextParagraph_) {
                            Token &newToken = tokens_.getNextAvailable();
                            newToken = {
                                .cStr = "\n",
                                .streamPlace = {lastStreamRelativeOffset_, fromPlace_.getFileIdx()},
                                .width = 0,
                                //.height = 0,
                                .length = 1,
                                .tokenId = TokenId::ENDPARAGRAPH,
                                .spacesAfter = 0};
                            lastToken_ = nullptr;

                            if (addTokenToPage(newToken)) {
                                return false;
                            }
                        }
                        break;

                    case Tag::BREAK:
                        // put a NEWLINE
                        if (!tokens_.isFull()) {
                            Token &newToken = tokens_.getNextAvailable();
                            newToken = {
                                .cStr = "\n",
                                .streamPlace = {lastStreamRelativeOffset_, fromPlace_.getFileIdx()},
                                .width = 0,
                                //.height = 0,
                                .length = 1,
                                .tokenId = TokenId::NEWLINE,
                                .spacesAfter = 0};
                            lastToken_ = nullptr;
                            if (addTokenToPage(newToken)) {
                                return false;
                            }
                        }
                        break;

                    default:
                        break;
                    }
                }
            } else {
                // pager_.completeLastLine();
                return false;
            }
        }
    } else if (node.type() == pugi::node_pcdata) {
        // log_w("Interpreting content %s", node.value());

        const pugi::char_t *str = node.value();
        uint32_t offset = node.offset_debug(); // stream_->getOffset(str);
        uint32_t len = strlen(str); // don't like that. Lenght not available from pugixml...

        if (!started_) {
            if ((offset + len) >= fromPlace_.getOffset()) {
                started_ = true;
                uint32_t bypass =
                    (fromPlace_.getOffset() > offset) ? fromPlace_.getOffset() - offset : 0;
                return addStrToPage(offset + bypass, str + bypass, len - bypass);
            }
        } else {
            return addStrToPage(offset, str, len);
        }
    }

    return true;
}

auto EPubTokenizer::addStrToPage(uint32_t offset, const pugi::char_t *str, uint32_t len) -> bool {

    uint32_t streamRelativeOffset = offset;

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

    // if constexpr (RENDERER_DEBUG) {
    //     log_d("First Character: %c (%02" PRIX8 "H)",
    //           (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);
    // }

    while ((!pageCompleted_) && (curChar != 0) && (!tokens_.isFull())) {

        // if constexpr (RENDERER_DEBUG) {
        //     log_d("Set the stream place to %s",
        //           addOffset(fromPlace, streamRelativeOffset).str().c_str());
        //     log_d("Current Character: %c (%02" PRIX8 "H)",
        //           (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);
        // }

        if (curChar > ' ') {
            Token &newToken = tokens_.getNextAvailable();

            newToken = {.cStr = nullptr,
                        .streamPlace = {streamRelativeOffset, fromPlace_.getFileIdx()},
                        .width = 0,
                        //.height = 0,
                        .length = 0,
                        .tokenId = TokenId::TEXT,
                        .spacesAfter = 0};
            lastToken_ = &newToken;

            if (curChar == '-') {
                nextByte();
                if (curChar != '-') {
                    newToken.tokenId = TokenId::HYPHEN;
                    pageCompleted_ = addTokenToPage(newToken, false, "-", 1);
                } else {
                    nextByte();
                    if (curChar != '-') {
                        newToken.tokenId = TokenId::ENDASH;
                        pageCompleted_ = addTokenToPage(newToken, false, "--", 2);
                    } else {
                        nextByte();
                        newToken.tokenId = TokenId::EMDASH;
                        pageCompleted_ = addTokenToPage(newToken, false, "---", 3);
                    }
                }
            } else {
                uint16_t textCharCount = 0;

                do {
                    // log_d("Current Character: %c (%02" PRIX8 "H)",
                    //       (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);

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

                tokens_.endOfCStr();

                // The last test of the while above is to get a complete UTF8 multi-bytes
                // character if the maximum word size is reached

                //  log_w("Text Token: %s", newToken.cStr);
                pageCompleted_ = addTokenToPage(newToken);
            }
        } else if (curChar == ' ') {

            if (lastToken_ != nullptr) {
                lastToken_->spacesAfter++;
                pager_.addSpaceWidth();
            }
            nextByte();
        } else if (curChar == '\n') {
            Token &newToken = tokens_.getNextAvailable();

            lastToken_ = nullptr;

            newToken = {.cStr = nullptr,
                        .streamPlace = {streamRelativeOffset, fromPlace_.getFileIdx()},
                        .width = 0,
                        //.height = 0,
                        .length = 0,
                        .tokenId = TokenId::NEWLINE,
                        .spacesAfter = 0};

            pageCompleted_ = addTokenToPage(newToken, false, "\n", 1);
            if (!pageCompleted_) {
                nextByte();
                if (curChar == '\n') {
                    newToken.tokenId = TokenId::ENDPARAGRAPH;
                    pageCompleted_ = addTokenToPage(newToken, false, "\n", 1);
                    nextByte();
                    while (curChar == '\n') {
                        nextByte();
                    }
                }
            }
        } else if (curChar == '\t') {
            Token &newToken = tokens_.getNextAvailable();

            newToken = {.cStr = nullptr,
                        .streamPlace = {streamRelativeOffset, fromPlace_.getFileIdx()},
                        .width = 0,
                        //.height = 0,
                        .length = 0,
                        .tokenId = TokenId::TAB,
                        .spacesAfter = 0};

            pageCompleted_ = addTokenToPage(newToken, false, "\t", 1);
            lastToken_ = &newToken;

            nextByte();
        }
    }

    if (!pageCompleted_ && tokens_.cStrIsFull()) {
        // There is a potential that the last token is not complete (e.g. a TEXT or ENDASH or
        // EMDASH), so forget it
        tokens_.forgetLast();
    }

    if constexpr (RENDERER_DEBUG) {
        log_d("Computed Word Count: %" PRIu16, pager_.getWordCount());
        log_d("Tokens Count: %" PRIu16, tokens_.getTokenCount());
    }

    lastStreamRelativeOffset_ = streamRelativeOffset;

    if (tokens_.isFull()) {
        log_e("Tokens_[] is full");
        if constexpr (ON_ERROR_EXIT) {
            exit(1);
        }
    }

    return !pageCompleted_;
}