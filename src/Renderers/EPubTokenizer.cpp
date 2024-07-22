#include "EPubTokenizer.hpp"

#include <string>

// clang-format off

// ToDo: Some cleanup required... there maybe more tags than required here

EPubTokenizer::Tags EPubTokenizer::tags
  = {{"p",                   Tag::P}, {"div",       Tag::DIV}, {"span",     Tag::SPAN}, 
     {"br",              Tag::BREAK}, {"h1",         Tag::H1}, {"h2",         Tag::H2}, 
     {"h3",                 Tag::H3}, {"h4",         Tag::H4}, {"h5",         Tag::H5}, 
     {"h6",                 Tag::H6}, {"b",           Tag::B}, {"i",           Tag::I},
     {"em",                 Tag::EM}, {"a",           Tag::A}, {"img",       Tag::IMG}, 
     {"picture",       Tag::PICTURE}, {"source", Tag::SOURCE}, {"li",         Tag::LI}, 
     {"blockquote", Tag::BLOCKQUOTE}, {"pre",       Tag::PRE}, {"strong", Tag::STRONG}, 
     {"figure",         Tag::FIGURE}, {"sup",       Tag::SUP}, {"hr",         Tag::HR},
     {"figcaption", Tag::FIGCAPTION}, {"*",         Tag::ANY}, {"ol",         Tag::OL},
     {"header",         Tag::HEADER}, {"sub",       Tag::SUB}, {"ul",         Tag::UL},
     {"table",           Tag::TABLE}, {"th",         Tag::TH}, {"tr",         Tag::TR},
     {"td",                 Tag::TD}, {"thead",   Tag::THEAD}
    };

// ToDo: Only put the ones that are defined in the Sol Font

EPubTokenizer::Entities EPubTokenizer::entities 
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

#if CONFIG_FONT_IBMF
EPubTokenizer::Characters EPubTokenizer::superscriptCharacters
{
    {"0", "\xE2\x81\xB0"},
    {"1", "\xC2\xB9"},    
    {"2", "\xC2\xB2"},    
    {"3", "\xC2\xB3"},    
    {"4", "\xE2\x81\xB4"},
    {"5", "\xE2\x81\xB5"},
    {"6", "\xE2\x81\xB6"},
    {"7", "\xE2\x81\xB7"},
    {"8", "\xE2\x81\xB8"},
    {"9", "\xE2\x81\xB9"},

    {"A", "\xEE\x81\x81"}, 
    {"B", "\xEE\x81\x82"}, 
    {"C", "\xEE\x81\x83"}, 
    {"D", "\xEE\x81\x84"}, 
    {"E", "\xEE\x81\x85"}, 
    {"F", "\xEE\x81\x86"}, 
    {"G", "\xEE\x81\x87"}, 
    {"H", "\xEE\x81\x88"}, 
    {"I", "\xEE\x81\x89"}, 
    {"J", "\xEE\x81\x8A"}, 
    {"K", "\xEE\x81\x8B"}, 
    {"L", "\xEE\x81\x8C"}, 
    {"M", "\xEE\x81\x8D"}, 
    {"N", "\xEE\x81\x8E"}, 
    {"O", "\xEE\x81\x8F"}, 
    {"P", "\xEE\x81\x90"}, 
    {"Q", "\xEE\x81\x91"}, 
    {"R", "\xEE\x81\x92"}, 
    {"S", "\xEE\x81\x93"}, 
    {"T", "\xEE\x81\x94"}, 
    {"U", "\xEE\x81\x95"}, 
    {"V", "\xEE\x81\x96"}, 
    {"W", "\xEE\x81\x97"}, 
    {"X", "\xEE\x81\x98"}, 
    {"Y", "\xEE\x81\x99"}, 
    {"Z", "\xEE\x81\x9A"},

    {"a", "\xEE\x81\x9F"}, 
    {"b", "\xEE\x81\xA0"}, 
    {"c", "\xEE\x81\xA1"}, 
    {"d", "\xEE\x81\xA2"}, 
    {"e", "\xEE\x81\xA3"}, 
    {"f", "\xEE\x81\xA4"}, 
    {"g", "\xEE\x81\xA5"}, 
    {"h", "\xEE\x81\xA6"}, 
    {"i", "\xEE\x81\xA7"}, 
    {"j", "\xEE\x81\xA8"}, 
    {"k", "\xEE\x81\xA9"}, 
    {"l", "\xEE\x81\xAA"}, 
    {"m", "\xEE\x81\xAB"}, 
    {"n", "\xEE\x81\xAC"}, 
    {"o", "\xEE\x81\xAD"}, 
    {"p", "\xEE\x81\xAE"}, 
    {"q", "\xEE\x81\xAF"}, 
    {"r", "\xEE\x81\xB0"}, 
    {"s", "\xEE\x81\xB1"}, 
    {"t", "\xEE\x81\xB2"}, 
    {"u", "\xEE\x81\xB3"}, 
    {"v", "\xEE\x81\xB4"}, 
    {"w", "\xEE\x81\xB5"}, 
    {"x", "\xEE\x81\xB6"}, 
    {"y", "\xEE\x81\xB7"}, 
    {"z", "\xEE\x81\xB8"}, 

    {"*", "\xEE\x81\xB9"},
    {"+", "\xE2\x81\xBA"},
    {"-", "\xE2\x81\xBB"},
    {"=", "\xE2\x81\xBC"},
    {"(", "\xE2\x81\xBD"},
    {")", "\xE2\x81\xBE"},
    {"[", "\xEE\x81\xBB"},
    {"]", "\xEE\x81\xBC"},
    {",", "\xEE\x81\x9B"},
    {".", "\xEE\x81\x9C"},
    {"/", "\xEE\x81\x9D"},
};

EPubTokenizer::Characters EPubTokenizer::subscriptCharacters
{
    {"0", "\xE2\x82\x80"},
    {"1", "\xE2\x82\x81"},
    {"2", "\xE2\x82\x82"},
    {"3", "\xE2\x82\x83"},
    {"4", "\xE2\x82\x84"},
    {"5", "\xE2\x82\x85"},
    {"6", "\xE2\x82\x86"},
    {"7", "\xE2\x82\x87"},
    {"8", "\xE2\x82\x88"},
    {"9", "\xE2\x82\x89"},

    {"A", "\xEE\x80\x85"},
    {"B", "\xEE\x80\x86"},
    {"C", "\xEE\x80\x87"},
    {"D", "\xEE\x80\x88"},
    {"E", "\xEE\x80\x89"},
    {"F", "\xEE\x80\x8A"},
    {"G", "\xEE\x80\x8B"},
    {"H", "\xEE\x80\x8C"},
    {"I", "\xEE\x80\x8D"},
    {"J", "\xEE\x80\x8E"},
    {"K", "\xEE\x80\x8F"},
    {"L", "\xEE\x80\x90"},
    {"M", "\xEE\x80\x91"},
    {"N", "\xEE\x80\x92"},
    {"O", "\xEE\x80\x93"},
    {"P", "\xEE\x80\x94"},
    {"Q", "\xEE\x80\x95"},
    {"R", "\xEE\x80\x96"},
    {"S", "\xEE\x80\x97"},
    {"T", "\xEE\x80\x98"},
    {"U", "\xEE\x80\x99"},
    {"V", "\xEE\x80\x9A"},
    {"W", "\xEE\x80\x9B"},
    {"X", "\xEE\x80\x9C"},
    {"Y", "\xEE\x80\x9D"},
    {"Z", "\xEE\x80\x9E"},

    {"a", "\xEE\x80\xA3"},
    {"b", "\xEE\x80\xA4"},
    {"c", "\xEE\x80\xA5"},
    {"d", "\xEE\x80\xA6"},
    {"e", "\xEE\x80\xA7"},
    {"f", "\xEE\x80\xA8"},
    {"g", "\xEE\x80\xA9"},
    {"h", "\xEE\x80\xAA"},
    {"i", "\xEE\x80\xAB"},
    {"j", "\xEE\x80\xAC"},
    {"k", "\xEE\x80\xAD"},
    {"l", "\xEE\x80\xAE"},
    {"m", "\xEE\x80\xAF"},
    {"n", "\xEE\x80\xB0"},
    {"o", "\xEE\x80\xB1"},
    {"p", "\xEE\x80\xB2"},
    {"q", "\xEE\x80\xB3"},
    {"r", "\xEE\x80\xB4"},
    {"s", "\xEE\x80\xB5"},
    {"t", "\xEE\x80\xB6"},
    {"u", "\xEE\x80\xB7"},
    {"v", "\xEE\x80\xB8"},
    {"w", "\xEE\x80\xB9"},
    {"x", "\xEE\x80\xBA"},
    {"y", "\xEE\x80\xBB"},
    {"z", "\xEE\x80\xBC"},

    {"*", "\xEE\x80\xBD"},
    {"+", "\xE2\x82\x8A"},
    {"-", "\xE2\x82\x8B"},
    {"=", "\xE2\x82\x8C"},
    {"(", "\xE2\x82\x8D"},
    {")", "\xE2\x82\x8E"},
    {"[", "\xEE\x80\xBF"},
    {"]", "\xEE\x81\x80"},
    {",", "\xEE\x80\x9F"},
    {".", "\xEE\x80\xA0"},
    {"/", "\xEE\x80\xA1"},
};
#endif

// clang-format on

auto EPubTokenizer::searchStart(pugi::xml_node node) -> uint32_t {
    uint32_t offset = 0;
    if ((node.type() == pugi::xml_node_type::node_pcdata) &&
        !EPubTokenizer::isSpaces(node.value())) {
        // log_w("[%s]...", node.value());
        const pugi::char_t *v = node.value();
        offset = node.offset_debug();
        while ((*v != '\0') && isspace(*v)) {
            v++;
            offset++;
        }
        if (*v != '\0') {
            return offset;
        }
    } else if (node.type() == pugi::xml_node_type::node_element) {
        // log_w("<%s>...", node.name());
        auto tagIt = tags.find(node.name());
        if (tagIt != tags.end()) {
            switch (tagIt->second) {
            case Tag::PICTURE:
                return node.offset_debug();
            case Tag::SUP:
            case Tag::SUB:
                return 0;
            default:
                break;
            }
        }

        for (auto subNode : node.children()) {
            offset = searchStart(subNode);
            if (offset != 0) {
                return offset;
            }
        }
    }
    return 0;
}

// Find the offset where the first element to be put on a page is located.
auto EPubTokenizer::computeStartingOffset(StreamPlace streamPlace) -> uint32_t {

    pugi::xml_document &doc = loadXHTMLAt(streamPlace.getSpineIdx());

    uint32_t offset = 0;
    for (auto node : doc.child("html").child("body").children()) {
        offset = searchStart(node);
        if (offset != 0) {
            break;
        }
    }
    return offset == 0 ? END_OFFSET : offset;
}

// Checks if a node is empty (i.e. there is no displayable
// character and no picture node in it), return true if so.
auto EPubTokenizer::isEmpty(pugi::xml_node node) -> bool {

    struct Walker : pugi::xml_tree_walker {
        auto for_each(pugi::xml_node &node) -> bool override {
            if (node.type() == pugi::xml_node_type::node_pcdata) {
                if (!EPubTokenizer::isSpaces(node.value())) {
                    empty = false;
                    return false;
                }
            } else if ((node.type() == pugi::xml_node_type::node_element) &&
                       (strcmp(node.name(), "span") != 0)) {
                empty = false;
                return false;
            }
            return true;
        }

        bool empty{};
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

    REND_WLOG("StreamPlace Offset: %" PRIu32 " vs fileStartingOffset: %" PRIu32,
              streamPlace.getOffset(), fileStartingOffset);

    if (streamPlace.getOffset() <= fileStartingOffset) {
        if (streamPlace.getSpineIdx() > 0) {
            return {0, static_cast<uint16_t>(streamPlace.getSpineIdx() - 1)};
        } else {
            return streamPlace; // don't move
        }
    } else {
        return {0, streamPlace.getSpineIdx()};
    }
}

auto EPubTokenizer::toUtf8(uint32_t val) -> char * {
    char *s = utf8Ch_;
    if (val <= 0x7F) {
        // Plain ASCII
        *s++ = static_cast<char>(val);
        *s = 0;
    } else if (val <= 0x07FF) {
        // 2-byte unicode
        *s++ = static_cast<char>(((val >> 6) & 0x1F) | 0xC0);
        *s++ = static_cast<char>(((val >> 0) & 0x3F) | 0x80);
        *s = 0;
    } else if (val <= 0xFFFF) {
        // 3-byte unicode
        *s++ = static_cast<char>(((val >> 12) & 0x0F) | 0xE0);
        *s++ = static_cast<char>(((val >> 6) & 0x3F) | 0x80);
        *s++ = static_cast<char>(((val >> 0) & 0x3F) | 0x80);
        *s = 0;
    } else if (val <= 0x10FFFF) {
        // 4-byte unicode
        *s++ = static_cast<char>(((val >> 18) & 0x07) | 0xF0);
        *s++ = static_cast<char>(((val >> 12) & 0x3F) | 0x80);
        *s++ = static_cast<char>(((val >> 6) & 0x3F) | 0x80);
        *s++ = static_cast<char>(((val >> 0) & 0x3F) | 0x80);
        *s = 0;
    } else {
        // error - use replacement character
        *s++ = static_cast<char>(0xEF);
        *s++ = static_cast<char>(0xBF);
        *s++ = static_cast<char>(0xBD);
        *s = 0;
    }
    return utf8Ch_;
}

// From the string that follow the '&' character, retrieve the entity's UTF8 or the unicode UTF8
// corresponding to the decimal (&#...;) or hexadecimal number (&#x...;). It returns the
// corresponding UTF8 translation and the next position in the input string.
auto EPubTokenizer::getCharEntity(const char *str) -> std::pair<const char *, const char *> {
    unsigned int len = 0;
    bool number = *str == '#';
    if (number) {
        str++;
    }
    bool hexNumber = *str == 'x';
    if (hexNumber) {
        str++;
    }
    const char *start = str;
    while ((len++ < 10) && (*str != '\0') && (*str != ';')) {
        str++;
    }
    std::string theStr = std::string(start, str - start);
    if (*str == ';') {
        str++;
        if (hexNumber) {
            return {toUtf8(strtol(theStr.c_str(), nullptr, 16)), str};
        } else if (number) {
            return {toUtf8(strtol(theStr.c_str(), nullptr, 10)), str};
        } else {
            auto search = entities.find(static_cast<SpiramString>(theStr));
            if (search != entities.end()) {
                return {search->second, str};
            }
        }
    }

    return {"", str};
}

auto EPubTokenizer::loadXHTMLAt(Idx spineIdx) -> pugi::xml_document & {
    const EPubOpf::SpineItem &spineItem = stream_->getSpine(spineIdx);
    return stream_->getXHTMLFile(static_cast<std::string>(spineItem.item->href));
}

#if 0
// Keep it just in case it is required again...
// 
// Find the next offset where the first character/picture will come from for the next page. This is
// required to synchronize a backward search: The last location of a page could be at a different
// location as the first location of next page because of xml in-between elements

auto EPubTokenizer::forwardToNextPageOffset(pugi::xml_node node) const -> uint32_t {

    struct Walker : pugi::xml_tree_walker {
        auto for_each(pugi::xml_node &node) -> bool override {
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

    uint32_t theOffset = walker.started ? walker.offset : END_OFFSET;

    REND_WLOG("Computed offset: %" PRIu32 ", search from offset: %" PRIu32, theOffset,
            fromPlace_.getOffset());

    return theOffset;
}

#endif

// Tokenize the document (through the Tokens class instance) and arrange the stream of tokens in
// lines and pages (through the Pager class).
//
// Can deal with forward and backward displacements. Page breaks are done between xhtml files
// that are commonly separated into chapters.
//
// As the XML parser generates a DOM like structure of a xhtml file, every call to this method
// will start from the beginning of the DOM toward the end of the targetted page.
//
// For forward displacements, the fromPlace argument is the location where the page will start.
// The algorithm will go as quick as possible to that location and start tokenizing there.
//
// For backward displacements, parsing starts at the beginning of the DOM and will end when the
// toPlace argument is reached (it is the start of the following page).
//
// ----
//
// Here are the elements that constitute the global state of the tokenizer. This state must be
// maintained up to date in both forward and backward displacements. The objectives are
//
// 1) To control when it's time to start creating tokens
//
// 2) To permit the page to be displayed to have proper indentation
//
// 3) To limit empty vertical space at beginning of pages
//
// 4) To reconduct the last token not part of the preceding page
//
//    As the process of generating a page uses a one token look-ahead approach, reconduct the
//    last token processed by the pager at the end of the preceding page to be the first one on
//    the new page
//
// 5) To have header paragraphs (<h1> .. <h6>) not being orphaned
//
//    Ensure that headers are displayed on the same page of the following first
//    line of text. A TokenId::HTEXT will be used for the first header word.
//
// 6) To have spaces properly added between text tokens (words)
//
// 7) To have supercript/subscript properly processed
//
//    As superscripts/subscripts consitute special kind of words, the state of displaying them
//    must be propagated through the interpreter recursion
//
// 8) To add brackets around hyperlinks
//
// 9) To maintain the current bytes offset in the current chapter
//
// 10) To remove paragraph indentation in tables' rows
//
// 11) To complete last word token in pcdata
//
//     The last word present in a string needs to be completed only when it is safe to do so.
//     Some formatting tags like <b>, <i>, <u> may cut a word, having an impact of misplaced
//     newlines if the first part of a word is found at the end of a line.
//
// 12) When the page to be displayed is ready
//
// 13) If menu parameters must be retrieved or not
//
// ----

auto EPubTokenizer::loadAndTokenizePage(StreamPlace fromPlace, bool backwards, StreamPlace *toPlace,
                                        bool retrieveMenuParameters)
    -> std::pair<StreamPlace, StreamPlace> {

    // if (stream_->isAnArticle()) log_w("article");

    // Not pretty, but limits the number of parameters to pass to the interpret() method as it is
    // recursively called
    fromPlace_ = fromPlace;
    toPlace_ = toPlace;
    backwards_ = backwards;

    REND_WLOG("Opening file at streamPlace %s, backwards: %s%s", fromPlace.str().c_str(),
              (backwards ? "YES, Up to: " : "NO"),
              (toPlace == nullptr ? "" : toPlace->str().c_str()));

    // ----> 1) to control when it's time to start creating tokens

    tokenizingStarted_ = backwards;
    currentStreamOffset_ = 0;

    // ----> 2) to permit the page to be displayed to have proper indentation

    listLevel_ = -1;

    savedBlockquoteLeftIndent_ = 0;
    savedBlockquoteRightIndent_ = 0;
    blockquoteLevel_ = -1;

    savedRowIndent_ = 0;

    // ----> 3) to limit empty vertical space at beginning of pages

    beginningOfPage_ = backwards;

    // ----> 4) To reconduct the last token not part of the preceding page

    // ----> 5) To have header paragraphs (<h1> .. <h6>) not being orphaned

    startOfHeader_ = false;

    // ----> 6) To have spaces properly added between text tokens (words)

    lastToken_ = nullptr; // To which token spaces will be attached to (as a count)

    // ----> 7) To have supercript/subscript properly processed

    superscript_ = false;
    subscript_ = false;

    // ----> 8) To add brackets around hyperlinks

    preText_[0] = postText_[0] = '\0';

    // ----> 9) To maintain the current bytes offset in the current chapter

    currentTxtOffset_ = 0;
    currentPageTxtOffset_ = 0;

    // ----> 10) To remove paragraph indentation in tables' rows

    inTableRow_ = false;

    // ----> 11) To complete last word token in pcdata

    textToken_ = nullptr;

    // ----> 12) When the page to be displayed is ready

    pageCompleted_ = false;

    // ----> 13) If menu parameters must be retrieved or not

    retrieveMenuParameters_ = retrieveMenuParameters;

    // ---

    if (stream_->isAnArticle()) {

        REND_WLOG("Reading an article...");

        currentSpineIdx_ = 0;

        REND_PROFILE_START(tokenizeProcess);

        bool completed = false;

        do {
            currentStreamOffset_ = 0;

            REND_PROFILE_START(loadXHTMLFile)
            pugi::xml_document &doc = loadXHTMLAt(currentSpineIdx_);
            REND_PROFILE_END(loadXHTMLFile)

            body_ = doc.child("html").child("body");
            pugi::xml_node node = body_.first_child();

            if (node) {
                do {
                    if (!interpret(node)) {
                        completed = true;
                    } else {
                        node = node.next_sibling();
                    }
                } while (node && !completed);
            }

            if (!node) {
                if (!completed) {
                    if (++currentSpineIdx_ >= stream_->getSpineCount()) {
                        if (currentPageTxtOffset_ <= currentTxtOffset_) {
                            currentTxtOffset_ -= currentPageTxtOffset_;
                        } else {
                            currentTxtOffset_ = 0;
                        }
                        REND_WLOG("End of Article, end of page: currentTxtOffset: %" PRIu32
                                  ", currentPageTxtOffset: %" PRIu32,
                                  currentTxtOffset_, currentPageTxtOffset_);
                        currentPageTxtOffset_ = 0;
                        pager_.setEndOfFile();
                        break;
                    }
                } else {
                    if ((fromPlace.getSpineIdx() + 1) < stream_->getSpineCount()) {
                        pager_.setNextPagePlace(
                            {0, static_cast<uint16_t>(fromPlace.getSpineIdx() + 1)});
                    }
                }
            }

        } while (!completed);

        REND_PROFILE_END(tokenizeProcess);

    } else {

        REND_WLOG("Reading a book...");

        currentSpineIdx_ = stream_->getChapterFirstSpineIdx(fromPlace.getSpineIdx());

        REND_PROFILE_START(tokenizeProcess);

        bool completed = false;

        do {
            currentStreamOffset_ = 0;

            REND_PROFILE_START(loadXHTMLFile)
            pugi::xml_document &doc = loadXHTMLAt(currentSpineIdx_);
            REND_PROFILE_END(loadXHTMLFile)

            body_ = doc.child("html").child("body");
            pugi::xml_node node = body_.first_child();

            if (node) {
                do {
                    if (!interpret(node)) {
                        completed = true; // The page is complete
                    } else {
                        node = node.next_sibling();
                    }
                } while (node && !completed);
            }

            if (!node) {
                // We are at the end of the current file
                // Pointing at next file, we will try to complete the current
                // page if it was not completely processed by the current file.
                currentSpineIdx_ += 1;

                // We break the process of paging if
                // - we are passed the end of the book
                // - the new current file is the beginning of a chapter
                if (stream_->spineIdxIsChapterStart(currentSpineIdx_)) {
                    log_w("We are at the beginning of a new chapter...");
                }
                if (!completed) {
                    if ((currentSpineIdx_ >= stream_->getSpineCount()) ||
                        (stream_->spineIdxIsChapterStart(currentSpineIdx_))) {
                        if (currentSpineIdx_ >= stream_->getSpineCount()) {
                            if (currentPageTxtOffset_ <= currentTxtOffset_) {
                                currentTxtOffset_ -= currentPageTxtOffset_;
                            } else {
                                currentTxtOffset_ = 0;
                            }
                            REND_WLOG("End of Chapter, end of page: currentTxtOffset: %" PRIu32
                                      ", currentPageTxtOffset: %" PRIu32,
                                      currentTxtOffset_, currentPageTxtOffset_);
                            currentPageTxtOffset_ = 0;
                            pager_.setEndOfFile();
                            break;
                            // } else {
                            //     pager_.completeLastLine();
                            //     break;
                        } else {
                            completed = true;
                            pager_.setNextPagePlace({0, currentSpineIdx_});
                        }
                    }
                } else {
                    if (currentSpineIdx_ < stream_->getSpineCount()) {
                        pager_.setNextPagePlace({0, currentSpineIdx_});
                    }
                }
            }

        } while (!completed);

        REND_PROFILE_END(tokenizeProcess);
    }

    REND_WLOG("End of job: Page start: %s, next page at: %s",
              pager_.getStreamPlaceOfFirstLine().str().c_str(),
              pager_.getNextPageStreamPlace().str().c_str());

    return {pager_.getStreamPlaceOfFirstLine(), pager_.getNextPageStreamPlace()};
}

auto EPubTokenizer::toStreamPlace(const std::shared_ptr<Place> place) const -> StreamPlace {
    if (place->isFor(DocType::EPUB)) {
        std::shared_ptr<EPubPlace> ePubPlace = std::dynamic_pointer_cast<EPubPlace>(place);
        int spineIdx = stream_->getSpineIdx(ePubPlace->getHRef());
        if (spineIdx == -1) {
            return getPlaceAtBeginningOfDocument();
        }
        return {place->getOffset(), spineIdx};
    } else {
        log_e("Wrong Place Type: %" PRIu8, static_cast<uint8_t>(place->getDocType()));
        REND_EXIT_SIM;
    }

    return {};
}

void EPubTokenizer::processPictureTag(pugi::xml_node node) {

    pugi::xml_node source = node.find_child_by_attribute("source", "class", "dithered");

    if (source) {
        Token &pictureToken = tokens_.getNextAvailable();
        pictureToken = {.cStr = nullptr,
                        .picture = nullptr,
                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                        .pixWidth = 0,
                        .pixHeight = 0,
                        .length = 0,
                        .tokenId = TokenId::PICTURE,
                        .spacesAfter = 0};

        std::string pictureFilePath = stream_->getRelativeFilePath(
            source.attribute("srcset").value(), fromPlace_.getSpineIdx());
        tokens_.addStr(pictureToken, pictureFilePath);

        pictureToken.pixWidth = pictureToken.pixHeight = 0;

        // Optimization: if <source> contains the width and height, the picture will
        // be loaded at the page generation time.
        if (source.attribute("width") && source.attribute("height")) {
            pictureToken.pixWidth = strtol(source.attribute("width").value(), nullptr, 10);
            pictureToken.pixHeight = strtol(source.attribute("height").value(), nullptr, 10);
            pictureToken.picture = nullptr;
        } else {
            REND_ELOG("Loading picture at the wrong moment!");

            REND_PROFILE_START(ReadPictureFromEPub);
            auto [buffer, len] = stream_->getFile(pictureFilePath);
            REND_PROFILE_END(ReadPictureFromEPub);

            if (buffer != nullptr) {
                pictureToken.picture = std::make_shared<BMPImage>();
                if (!pictureToken.picture->readFromBytes(buffer.get(), len)) {
                    log_e("Unable ro retrieve the BMP picture from EPub");
                    REND_EXIT_SIM;
                } else {
                    pictureToken.pixHeight = pictureToken.picture->getHeight();
                    pictureToken.pixWidth = pictureToken.picture->getWidth();
                }
            } else {
                REND_ELOG("buffer is empty!");
            }
        }

        if (((pictureToken.pixHeight +
              pager_.deciToPix(NEWLINE_DECI_LINE_HEIGHT + ENDPARAGRAPH_DECI_LINE_HEIGHT))) <
            rect_.height) {
            pictureToken.tokenId = TokenId::EPICTURE;
        }

        lastToken_ = nullptr;

        // After the following call, pictureToken may become irrelevant
        addTokenToPage(pictureToken);
        REND_WLOG("After picture processing, pageCompleted_: %s", pageCompleted_ ? "Yes" : "No");
    }
}

// Start a new page. As the token parameter is a reference into the tokens_[] array that was not
// injected in last page, we need to take a copy of it to re-inject into the tokens_[] after it
// was cleared (the token parameter was not accepted to be part of the previous page, so we need
// to keep it alive in the next page to come). This must also take care of the cStr pool pointer
// unless a str parameter is present, meaning a constant string that is outside of the pool.
void EPubTokenizer::startNewPageWithToken(Token &token, const char *str, uint8_t strSize) {

    Token tempToken;
    std::string tempStr;
    std::shared_ptr<BMPImage> picture = std::move(token.picture);
    if (str == nullptr && (token.length != 0)) {
        tempStr = token.cStr;
        token.cStr = nullptr;
        token.length = 0;
    }
    tempToken = token;
    tokens_.clear();
    pager_.clear();

    updatePagerIndent(tempToken.tokenId == TokenId::ITEXT);

    Token &newToken = tokens_.getNextAvailable();
    newToken = tempToken;
    newToken.picture = std::move(picture);

    // lastToken_ must have been properly set BEFORE a call to this method.
    if (lastToken_ != nullptr) {
        lastToken_ = &newToken;
    }
    if (!tempStr.empty()) {
        tokens_.addStr(newToken, tempStr);
    }

    // Recursive call
    // After the following call, newToken may become irrelevant
    addTokenToPage(newToken, str, strSize);
}

// Integrates the token to the tokens_[] array and to the page.
//
// Returns true if the page is ready.
//
// Takes into account the need, in the context of backward movements, to wait until the target
// location is reached. If the target location wasn't reached and the pager_ returns with a
// completed page signal (pager_.addLastTokenToPage(false) returns false), the last token
// entered is saved to be pushed back in the tokens_ stream. Note that the token parameter may
// then become irrelevant as it could have been moved (very high probability) through the
// process of initializing the objects for next page preparation.
auto EPubTokenizer::addTokenToPage(Token &token, const char *str, uint8_t strSize) -> bool {
    if (tokens_.add(token, str, strSize)) {
        // log_w("Token Id: %" PRIu8 " at idx: %" PRIu16, (uint8_t)token.tokenId,
        // tokens_.getLastIdx());
        if (tokens_.getTokenCount() == 1) {
            if (!backwards_) {
                // This is the first token added, so we are at the beginning of a page
                updatePagerIndent(token.tokenId == TokenId::ITEXT);
            } else {
                currentPageTxtOffset_ = token.length;
            }
        }

        if (!pager_.addLastTokenToPage(false)) {
            if ((static_cast<uint16_t>(token.tokenId) &
                 (static_cast<uint16_t>(TokenId::NEWLINE) |
                  static_cast<uint16_t>(TokenId::ENDPARAGRAPH))) == 0) {
                if (backwards_) {
                    if (token.streamPlace >= *toPlace_) {
                        // So we are at the location corresponding to the end of the requested
                        // page.
                        pageCompleted_ = true;
                        if (currentPageTxtOffset_ <= currentTxtOffset_) {
                            currentTxtOffset_ -= currentPageTxtOffset_;
                        } else {
                            currentTxtOffset_ = 0;
                        }
                        REND_WLOG("Backward end of page: currentTxtOffset: %" PRIu32
                                  ", currentPageTxtOffset: %" PRIu32,
                                  currentTxtOffset_, currentPageTxtOffset_);
                        currentPageTxtOffset_ = 0;
                    } else {
                        startNewPageWithToken(token, str, strSize);
                    }
                } else {
                    pageCompleted_ = true;
                    REND_WLOG("Forward end of page: currentTxtOffset: %" PRIu32
                              ", currentPageTxtOffset: %" PRIu32,
                              currentTxtOffset_, currentPageTxtOffset_);
                }
            }
        } else {
            beginningOfPage_ = false; // pager_.linesIsEmpty() && tokens_.isEmpty();
        }
    } else {

        log_e("Internal error. Tokens::tokens_[] circular array not expected to be full. Not "
              "large "
              "enough!");
        REND_EXIT_SIM;

        pageCompleted_ = true;
    }
    return pageCompleted_;
}

// pugixml doesnt supply the length of a tag in the buffer or the end-tag location. To find the
// offset, we have to find the next node that follow the one we are interested in through the
// sibling. If none exists, we have to go up in the chain of parents in the tree.
auto EPubTokenizer::findNodeEndOffset(pugi::xml_node node) -> uint32_t {
    while ((node == node.parent().last_child()) && (node != body_)) {
        node = node.parent();
    }
    uint32_t offset = (node == body_) ? END_OFFSET : node.next_sibling().offset_debug() - 1;
    return offset;
}

// Adjust pager indentation
void EPubTokenizer::updatePagerIndent(bool iText) {
    if (listLevel_ == -1) {
        pager_.setLineIndent(savedBlockquoteLeftIndent_ + savedRowIndent_,
                             savedBlockquoteRightIndent_);
        // } else if (iText) {
        //     if (listLevel_ > 0) {
        //         pager_.setLineIndent(listIndent_[listLevel_ - 1] + savedBlockquoteLeftIndent_ +
        //                                  savedRowIndent_,
        //                              savedBlockquoteRightIndent_);
        //     } else {
        //         pager_.setLineIndent(savedBlockquoteLeftIndent_ + savedRowIndent_,
        //                              savedBlockquoteRightIndent_);
        //     }
    } else {
        if (iText) {
            pager_.setLineIndent(listIndent_[listLevel_] + savedBlockquoteLeftIndent_ +
                                     savedRowIndent_,
                                 savedBlockquoteRightIndent_);
        } else {
            pager_.setLineIndent(listIndent_[listLevel_] + listUndent_[listLevel_] +
                                     savedBlockquoteLeftIndent_ + savedRowIndent_,
                                 savedBlockquoteRightIndent_);
        }
    }
}

// Retrieve menu parameters of the following form:
//
//    <ul><li id="key">value</li> ... </ul>
//
// Returns false if no valid menu parameter present.
auto EPubTokenizer::processMenuParameters(pugi::xml_node node) -> bool {
    if (node.type() != pugi::node_element) {
        return false;
    }

    auto tagIt = tags.find(node.name());

    if (tagIt->second != Tag::UL) {
        return false;
    }

    bool atLeastOnce = false;

    for (pugi::xml_node liNode : node.children("li")) {
        const pugi::char_t *key = liNode.attribute("id").value();
        const pugi::xml_text text = liNode.text();
        if ((key != nullptr) && !text.empty()) {
            atLeastOnce = true;
            pager_.addMenuParameter(key, text.as_string());
        }
    }

    return atLeastOnce;
}

// Process node content, going recursively for all its parts. Returns true if parsing must
// continue, false if its time to stop (We are at the end of a page (going forward) or the
// toPlace_ has been reached (going backward))
auto EPubTokenizer::interpret(pugi::xml_node node) -> bool {

    currentStreamOffset_ = node.offset_debug();

    if (node.type() == pugi::node_element) {

        // log_w("Interpreting node element <%s> at offset %" PRIu32, node.name(),
        //       currentStreamOffset_);

        if (!tokenizingStarted_) {
            if (((currentSpineIdx_ == fromPlace_.getSpineIdx()) &&
                 (currentStreamOffset_ >= fromPlace_.getOffset())) ||
                (currentSpineIdx_ > fromPlace_.getSpineIdx())) {
                tokenizingStarted_ = true;
                beginningOfPage_ = true;
                REND_WLOG("Started at [offset:%" PRIu32 ",spineIdx:%" PRIu16 "]",
                          currentStreamOffset_, currentSpineIdx_);
            }
        }

        auto tagIt = tags.find(node.name());

        // if (tagIt != tags_.end()) {
        //     log_e("Tag: %d (%s) at offset %" PRIu32, (int)tag, tagIt->first.c_str(),
        //     currentStreamOffset_);
        // }

        // Start of a node element processing

        if (tagIt != tags.end()) {

            if ((tagIt->second == Tag::DIV) &&
                (strcmp(node.attribute("id").value(), "sol-menu") == 0)) {
                if ((!backwards_) && tokenizingStarted_ && retrieveMenuParameters_ &&
                    processMenuParameters(node.first_child())) {
                    return false;
                }
                return interpret(node.next_sibling());
            }

            switch (tagIt->second) {
            case Tag::H1:
            case Tag::H2:
            case Tag::H3:
            case Tag::H4:
            case Tag::H5:
            case Tag::H6:
                startOfHeader_ = true;
                [[fallthrough]];
            case Tag::P:
            case Tag::DIV:
            case Tag::TD:
            case Tag::TH:
                if (tokenizingStarted_ && closeTextToken()) {
                    return false;
                }
                if (isEmpty(node)) {
                    startOfHeader_ = false;
                    return true;
                }
                break;

            case Tag::BLOCKQUOTE:
                if (tokenizingStarted_ && closeTextToken()) {
                    return false;
                }

                blockquoteLevel_ += 1;
                savedBlockquoteLeftIndent_ += (blockquoteLevel_ == 0) ? 12 : 8;
                savedBlockquoteRightIndent_ += 8;

                updatePagerIndent(false);
                break;

            case Tag::PICTURE:
                if (tokenizingStarted_) {
                    if (closeTextToken()) {
                        return false;
                    }
                    // log_w("Picture found...");
                    currentStreamOffset_ = node.offset_debug();
                    processPictureTag(node);
                    return !pageCompleted_;
                }
                currentStreamOffset_ = findNodeEndOffset(node);
                return true; // skip children

            case Tag::SUP:
                if (tokenizingStarted_ && closeTextToken()) {
                    return false;
                }
                superscript_ = true;
                break;

            case Tag::SUB:
                if (tokenizingStarted_ && closeTextToken()) {
                    return false;
                }
                subscript_ = true;
                break;

            case Tag::OL:
                if (tokenizingStarted_) {
                    if (closeTextToken()) {
                        return false;
                    }

                    if (listLevel_ >= 0) {
                        if (!beginningOfPage_) {
                            Token &newToken = tokens_.getNextAvailable();
                            newToken = {.cStr = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::NEWLINE,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, "\n", 1);
                        }
                    }
                }

                if (listLevel_ < (MAX_LEVEL_COUNT - 2)) {
                    listLevel_ += 1;
                    listType_[listLevel_] = ListType::ORDERED;
                    pugi::xml_attribute attr = node.attribute("start");
                    listSeqNbr_[listLevel_] = attr ? strtol(attr.value(), nullptr, 10) : 1;
                    listIndent_[listLevel_] = 0; // To be computed in <li> processing
                }
                break;

            case Tag::UL:
                if (tokenizingStarted_) {
                    if (closeTextToken()) {
                        return false;
                    }
                    if (listLevel_ >= 0) {
                        if (!beginningOfPage_) {
                            Token &newToken = tokens_.getNextAvailable();
                            newToken = {.cStr = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::NEWLINE,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, "\n", 1);
                        }
                    }
                }

                if (listLevel_ < (MAX_LEVEL_COUNT - 2)) {
                    listLevel_ += 1;
                    listType_[listLevel_] = ListType::UNORDERED;
                    listSeqNbr_[listLevel_] = 1;
                    listIndent_[listLevel_] = 0; // To be computed in <li> processing
                }
                break;

            case Tag::LI: {
                if (tokenizingStarted_ && closeTextToken()) {
                    return false;
                }

                char listLineStart[50];

                if (listLevel_ >= 0) {
                    if (listType_[listLevel_] == ListType::ORDERED) {
                        pugi::xml_attribute attr = node.attribute("value");
                        int32_t seqNbr =
                            attr ? strtol(attr.value(), nullptr, 10) : listSeqNbr_[listLevel_];
                        (void)snprintf(listLineStart, 49, "%" PRIi32 ".  ", seqNbr);
                        listSeqNbr_[listLevel_] = ++seqNbr;
                    } else {
                        (void)snprintf(listLineStart, 49, "%c  ", LEVEL_CHAR[listLevel_]);
                    }
                    listIndent_[listLevel_] = (listLevel_ * 15) + 5;
                    listUndent_[listLevel_] = display_.getTextWidth(listLineStart);
                }

                if (tokenizingStarted_ && (listLevel_ >= 0)) {
                    updatePagerIndent(true);
                    Token &newToken = tokens_.getNextAvailable();
                    // log_w("LI start: %s", listLineStart_);
                    newToken = {.cStr = nullptr,
                                .picture = nullptr,
                                .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                .pixWidth = 0,
                                .pixHeight = 0,
                                .length = 0,
                                .tokenId = TokenId::ITEXT,
                                .spacesAfter = 0};
                    tokens_.addStr(newToken, listLineStart);
                    lastToken_ = nullptr;

                    if (addTokenToPage(newToken)) {
                        return false;
                    }
                }
            } break;

            case Tag::A: {
                const pugi::char_t *v = node.child_value();
                if ((v[0] != '\0') && tokenizingStarted_ && closeTextToken()) {
                    return false;
                }
                if (!superscript_ && !subscript_ && (v[0] != '\0') && (v[0] != '(') &&
                    (v[0] != '[') && (v[0] != '<') && (v[0] != '{')) {
                    preText_[0] = '[';
                    postText_[0] = ']';
                    preText_[1] = postText_[1] = '\0';
                }
            } break;

            case Tag::TABLE:
                if (tokenizingStarted_) {
                    if (closeTextToken()) {
                        return false;
                    }
                    Token &newToken = tokens_.getNextAvailable();
                    newToken = {.cStr = nullptr,
                                .picture = nullptr,
                                .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                .pixWidth = 0,
                                .pixHeight = 0,
                                .length = 0,
                                .tokenId = TokenId::BTABLE,
                                .spacesAfter = 0};
                    lastToken_ = nullptr;

                    if (addTokenToPage(newToken, "{TABLE}", 7)) {
                        return false;
                    }
                }
                break;

            case Tag::TR:
                if (tokenizingStarted_) {
                    if (closeTextToken()) {
                        return false;
                    }
                    Token &newToken = tokens_.getNextAvailable();
                    newToken = {.cStr = nullptr,
                                .picture = nullptr,
                                .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                .pixWidth = 0,
                                .pixHeight = 0,
                                .length = 0,
                                .tokenId = TokenId::HRULE,
                                .spacesAfter = 0};
                    lastToken_ = nullptr;

                    if (addTokenToPage(newToken)) {
                        return false;
                    }
                }
                inTableRow_ = true;
                break;

            default:
                break;
            }
        }

        // Recurse on all children elements

        for (pugi::xml_node subNode : node.children()) {
            if (!interpret(subNode)) {
                return false;
            }
        }

        // End of the node element processing

        currentStreamOffset_ = findNodeEndOffset(node);
        // REND_WLOG("Offset at the end of the node: %s = %" PRIu32, node.name(),
        // currentStreamOffset_);

        if (!tokenizingStarted_) {
            if (((currentSpineIdx_ == fromPlace_.getSpineIdx()) &&
                 (currentStreamOffset_ >= fromPlace_.getOffset())) ||
                (currentSpineIdx_ > fromPlace_.getSpineIdx())) {
                tokenizingStarted_ = true;
                beginningOfPage_ = true;
                REND_WLOG("Started at [offset:%" PRIu32 ",spineIdx:%" PRIu16 "]",
                          currentStreamOffset_, currentSpineIdx_);
            }
        }

        if (!pageCompleted_) {
            if (tagIt != tags.end()) {
                switch (tagIt->second) {
                case Tag::H1:
                case Tag::H2:
                case Tag::H3:
                case Tag::H4:
                case Tag::H5:
                case Tag::H6:
                    startOfHeader_ = false; // Needed if going forward
                    [[fallthrough]];
                case Tag::P:
                case Tag::DIV:
                case Tag::HEADER:
                case Tag::TD:
                case Tag::TH:
                    if (tokenizingStarted_) {
                        if (closeTextToken()) {
                            return false;
                        }
                        if (!beginningOfPage_) {
                            Token &newToken = tokens_.getNextAvailable();
                            newToken = {.cStr = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId =
                                            inTableRow_ ? TokenId::NEWLINE : TokenId::ENDPARAGRAPH,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, "\n", 1);
                        }
                    }
                    break;

                case Tag::BLOCKQUOTE:
                    savedBlockquoteLeftIndent_ =
                        (savedBlockquoteLeftIndent_ > 0)
                            ? savedBlockquoteLeftIndent_ - ((blockquoteLevel_ == 0) ? 12 : 8)
                            : 0;
                    savedBlockquoteRightIndent_ =
                        (savedBlockquoteRightIndent_ > 0) ? savedBlockquoteRightIndent_ - 8 : 0;

                    if (blockquoteLevel_ >= 0) {
                        blockquoteLevel_--;
                    }

                    if (tokenizingStarted_) {
                        if (closeTextToken()) {
                            return false;
                        }
                        if (!beginningOfPage_) {
                            Token &newToken = tokens_.getNextAvailable();
                            newToken = {.cStr = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId =
                                            inTableRow_ ? TokenId::NEWLINE : TokenId::ENDPARAGRAPH,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, "\n", 1);
                        }
                    }
                    updatePagerIndent(false);
                    break;

                case Tag::SUP:
                    superscript_ = false;
                    if (tokenizingStarted_ && closeTextToken()) {
                        return false;
                    }
                    break;

                case Tag::SUB:
                    subscript_ = false;
                    if (tokenizingStarted_ && closeTextToken()) {
                        return false;
                    }
                    break;

                case Tag::BREAK:
                    if (tokenizingStarted_) {
                        if (closeTextToken()) {
                            return false;
                        }
                        // put a SPACE
                        if (lastToken_ != nullptr) {
                            lastToken_->spacesAfter++;
                            if (lastToken_->spacesAfter == 1) {
                                pager_.addSpaceWidth();
                            }
                        }
                    }
                    break;

                case Tag::HR:
                    if (tokenizingStarted_) {
                        if (closeTextToken()) {
                            return false;
                        }
                        Token &newToken = tokens_.getNextAvailable();
                        newToken = {.cStr = nullptr,
                                    .picture = nullptr,
                                    .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                    .pixWidth = 0,
                                    .pixHeight = 0,
                                    .length = 0,
                                    .tokenId = TokenId::HRULE,
                                    .spacesAfter = 0};
                        lastToken_ = nullptr;
                        if (addTokenToPage(newToken, "\n", 1)) {
                            return false;
                        }
                    }
                    break;

                case Tag::OL:
                    if (listLevel_ >= 0) {
                        listLevel_ -= 1;
                    }
                    if (tokenizingStarted_) {
                        if (closeTextToken()) {
                            return false;
                        }
                        if (!beginningOfPage_) {
                            Token &newToken = tokens_.getNextAvailable();
                            newToken = {.cStr = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::ENDPARAGRAPH,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, "\n", 1);
                        }
                    }
                    updatePagerIndent(false);
                    break;

                case Tag::UL:
                    if (listLevel_ >= 0) {
                        listLevel_ -= 1;
                    }
                    if (tokenizingStarted_) {
                        if (closeTextToken()) {
                            return false;
                        }
                        if (!beginningOfPage_) {
                            Token &newToken = tokens_.getNextAvailable();
                            newToken = {.cStr = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::ENDPARAGRAPH,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, "\n", 1);
                        }
                    }
                    updatePagerIndent(false);
                    break;

                case Tag::LI:
                    // log_w("</LI>");
                    if (tokenizingStarted_) {
                        if (closeTextToken()) {
                            return false;
                        }
                        if (!beginningOfPage_) {
                            Token &newToken = tokens_.getNextAvailable();
                            newToken = {.cStr = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::NEWLINE,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, "\n", 1);
                        }
                    }
                    break;

                case Tag::A:
                    if (tokenizingStarted_) {
                        if ((postText_[0] != '\0') && closeTextToken(true)) {
                            return false;
                        }
                        // if (postText_[0] != '\0') {
                        //     Token &token = tokens_.getLast();
                        //     tokens_.insertBytesAtEnd(token, postText_);
                        // }
                    }
                    preText_[0] = postText_[0] = '\0';
                    break;

                case Tag::TABLE:
                    if (tokenizingStarted_) {
                        if (closeTextToken()) {
                            return false;
                        }
                        Token &newToken = tokens_.getNextAvailable();
                        newToken = {.cStr = nullptr,
                                    .picture = nullptr,
                                    .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                    .pixWidth = 0,
                                    .pixHeight = 0,
                                    .length = 0,
                                    .tokenId = TokenId::ETABLE,
                                    .spacesAfter = 0};
                        lastToken_ = nullptr;

                        if (addTokenToPage(newToken, "{END TABLE}", 11)) {
                            return false;
                        }
                    }
                    break;

                case Tag::TR:
                    if (tokenizingStarted_) {
                        if (closeTextToken()) {
                            return false;
                        }
                    }
                    inTableRow_ = false;
                    break;

                default:
                    break;
                }
            }
        } else {
            return false;
        }
    } else if (node.type() == pugi::node_pcdata) {
        // log_w("Interpreting pcdata '%s'", node.value());

        const pugi::char_t *str = node.value();
        uint32_t len = strlen(str); // don't like that. Lenght not available from pugixml...
        // Count forward the number of non-tag bytes (for estimating page number)
        // currentTxtOffset_ = 0;

        if (!tokenizingStarted_) {
            if (((currentSpineIdx_ == fromPlace_.getSpineIdx()) &&
                 ((currentStreamOffset_ + len) >= fromPlace_.getOffset())) ||
                (currentSpineIdx_ > fromPlace_.getSpineIdx())) {
                tokenizingStarted_ = true;
                beginningOfPage_ = true;
                uint32_t bypass = (fromPlace_.getOffset() > currentStreamOffset_)
                                      ? fromPlace_.getOffset() - currentStreamOffset_
                                      : 0;

                REND_WLOG("Starting at %" PRIu32 ", bypass: %" PRIu32 ", length: %" PRIu32,
                          currentStreamOffset_ + bypass, bypass, len);

                currentStreamOffset_ += bypass;
                if (str[bypass] == '\0') {
                    preText_[0] = postText_[0] = '\0';
                    currentTxtOffset_ += len;
                    return true;
                } else {
                    if (bypass > 0) {
                        preText_[0] = '\0';
                    }
                    currentTxtOffset_ += bypass;
                    return addStrToPage(str + bypass, len - bypass);
                }
            } else {
                currentStreamOffset_ += len;
                currentTxtOffset_ += len;
            }
        } else {
            return addStrToPage(str, len);
        }
    }

    return true;
}

auto EPubTokenizer::addStrToPage(const pugi::char_t *str, uint32_t len) -> bool {

    uint32_t streamOffset = currentStreamOffset_;

    // REND_WLOG("Started at offset: %" PRIu32, streamOffset);

    uint8_t curChar = *str++;
    len--;

    // Keep it DRY

    auto nextByte = [&curChar, &str, &len, &streamOffset, this]() -> void {
        if (len == 0) {
            curChar = 0;
        } else {
            curChar = *str++;
            len--;
            streamOffset++;
            if (backwards_) {
                currentTxtOffset_++;
                currentPageTxtOffset_++;
            }
        }
    };

    if (beginningOfPage_) {
        while ((curChar != 0) && isspace(curChar)) {
            nextByte();
        }
    }

    if (curChar != 0) {
        beginningOfPage_ = false;
    }

    // REND_WLOG("First Character: %c (%02" PRIX8 "H)",
    //       (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);

    while (curChar != 0) {

        // REND_WLOG("Set the stream place to %s",
        //       addOffset(fromPlace, streamOffset).str().c_str());
        // REND_WLOG("Current Character: %c (%02" PRIX8 "H)",
        //       (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '), curChar);

#if CONFIG_FONT_IBMF
        if (superscript_) {
            superscript_ = false;
            Token &newToken = tokens_.getNextAvailable();
            newToken = {.cStr = nullptr,
                        .picture = nullptr,
                        .streamPlace = {streamOffset, currentSpineIdx_},
                        .pixWidth = 0,
                        .pixHeight = 0,
                        .length = 0,
                        .tokenId = TokenId::TEXT,
                        .spacesAfter = 0};
            do {
                auto chIt =
                    superscriptCharacters.find(SpiramString(reinterpret_cast<char *>(&curChar), 1));
                if (chIt != superscriptCharacters.end()) {
                    const char *script = chIt->second;
                    while (*script) {
                        if (!tokens_.cStrIsFull()) {
                            tokens_.addByte(newToken, *script++);
                        } else {
                            break;
                        }
                    }
                }
                nextByte();
            } while (curChar != 0);

            if (newToken.length > 0) {
                tokens_.endOfCStr();
                lastToken_ = &newToken; // Must be done before calling addTokenToPage()

                // After the following call, newToken may become irrelevant
                addTokenToPage(newToken);
            }

        } else if (subscript_) {
            subscript_ = false;
            Token &newToken = tokens_.getNextAvailable();
            newToken = {.cStr = nullptr,
                        .picture = nullptr,
                        .streamPlace = {streamOffset, currentSpineIdx_},
                        .pixWidth = 0,
                        .pixHeight = 0,
                        .length = 0,
                        .tokenId = TokenId::TEXT,
                        .spacesAfter = 0};
            do {
                auto chIt =
                    subscriptCharacters.find(SpiramString(reinterpret_cast<char *>(&curChar), 1));
                if (chIt != subscriptCharacters.end()) {
                    const char *script = chIt->second;
                    while (*script) {
                        if (!tokens_.cStrIsFull()) {
                            tokens_.addByte(newToken, *script++);
                        } else {
                            break;
                        }
                    }
                }
                nextByte();
            } while (curChar != 0);

            if (newToken.length > 0) {
                tokens_.endOfCStr();
                lastToken_ = &newToken; // Must be done before calling addTokenToPage()

                // After the following call, newToken may become irrelevant
                addTokenToPage(newToken);
            }

        } else
#endif
            if (curChar > ' ') {
#if CONFIG_FONT_TTF
            if (superscript_) {
                superscript_ = false;
                closeTextToken();
                textToken_ = &tokens_.getNextAvailable();
                *textToken_ = {.cStr = nullptr,
                               .picture = nullptr,
                               .streamPlace = {streamOffset, currentSpineIdx_},
                               .pixWidth = 0,
                               .pixHeight = 0,
                               .length = 0,
                               .tokenId = TokenId::SUPTEXT,
                               .spacesAfter = 0};

            } else if (subscript_) {
                subscript_ = false;
                closeTextToken();
                textToken_ = &tokens_.getNextAvailable();
                *textToken_ = {.cStr = nullptr,
                               .picture = nullptr,
                               .streamPlace = {streamOffset, currentSpineIdx_},
                               .pixWidth = 0,
                               .pixHeight = 0,
                               .length = 0,
                               .tokenId = TokenId::SUBTEXT,
                               .spacesAfter = 0};

            } else
#endif

                if (textToken_ == nullptr) {
                textToken_ = &tokens_.getNextAvailable();

                *textToken_ = {.cStr = nullptr,
                               .picture = nullptr,
                               .streamPlace = {streamOffset, currentSpineIdx_},
                               .pixWidth = 0,
                               .pixHeight = 0,
                               .length = 0,
                               .tokenId = TokenId::TEXT,
                               .spacesAfter = 0};
                textCharCount_ = 0;
            }

            if ((textToken_->length == 0) && (preText_[0] != '\0')) {
                tokens_.addBytes(*textToken_, preText_);
                preText_[0] = '\0';
            }

            if (curChar == '-') {
                textToken_->tokenId = startOfHeader_ ? TokenId::HTEXT : TokenId::HYPHEN;
                startOfHeader_ = false;
                tokens_.addByte(*textToken_, '-');
                nextByte();
                closeTextToken();
            } else {
                if (startOfHeader_) {
                    textToken_->tokenId = TokenId::HTEXT;
                    startOfHeader_ = false;
                }

                // Count the number of nbsp in a row, to limit the number of them that
                // could be present in a word. The current limit is 4
                const uint8_t nbspCountLimit = 4;
                uint8_t nbspCount = 0;

                do {
                    // log_d("Current Character: %c (%02" PRIX8 "H)",
                    //       (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '),
                    //       curChar);

                    if (curChar == '&') {
                        auto [trans, newStrLocation] = getCharEntity(str);
                        if (*trans != '\0') {
                            len -= (newStrLocation - str);
                            streamOffset += (newStrLocation - str);
                            if ((static_cast<uint8_t>(trans[0]) == 0xC2U) &&
                                (static_cast<uint8_t>(trans[1]) == 0xA0U)) {
                                // That is a nbsp character, limit to 4 in a row
                                if (nbspCount < nbspCountLimit) {
                                    nbspCount += 1;
                                    if (!tokens_.cStrIsFull()) {
                                        tokens_.addByte(*textToken_, ' ');
                                        textCharCount_ += 1;
                                    }
                                }
                            } else {
                                while (*trans && !tokens_.cStrIsFull()) {
                                    if (*trans > ' ') {
                                        tokens_.addByte(*textToken_, *trans);
                                    }
                                    trans++;
                                }
                                textCharCount_ += 1;
                                nbspCount = 0; // not a nbsp, reset counter
                            }

                            str = newStrLocation;
                            nextByte();
                        } else {
                            if (!tokens_.cStrIsFull()) {
                                tokens_.addByte(*textToken_, curChar);
                                textCharCount_ += 1;
                            } else {
                                break;
                            }
                            nbspCount = 0; // not a nbsp, reset counter
                            nextByte();
                        }
                    } else {
                        if ((curChar <= 0x7FU) || (curChar >= 0xC0U)) {
                            textCharCount_ += 1;
                        }
                        if (!tokens_.cStrIsFull()) {
                            if (curChar == 0xC2U) {
                                nextByte();
                                if (curChar == 0xA0U) {
                                    // This is a nbsp character, limit to 4 in a row
                                    if (nbspCount < nbspCountLimit) {
                                        nbspCount += 1;
                                        tokens_.addByte(*textToken_, ' ');
                                    } else {
                                        textCharCount_ -= 1;
                                    }
                                } else {
                                    tokens_.addByte(*textToken_, 0xC2U);
                                    tokens_.addByte(*textToken_, curChar);
                                    nbspCount = 0; // not a nbsp, reset counter
                                }
                            } else {
                                tokens_.addByte(*textToken_, curChar);
                                nbspCount = 0; // not a nbsp, reset counter
                            }
                        } else {
                            break;
                        }
                        if (curChar == '-') {
                            nextByte();
                            break;
                        }
                        nextByte();
                    }
                } while ((curChar > ' ') &&
                         ((textCharCount_ < maxTextTokenCharSize_) || ((curChar & 0xC0U) == 0x80)));

                // The last test of the while above is to get a complete UTF8 multi-bytes
                // character if the maximum word size is reached

                // if we reach the end of the pcdata, we keep the textToken open. It will be
                // completed with other pcdata, or by the next tag that is known to require that
                // all remainig text was processed. This is the case of all the tags that are
                // considered by the tokenizer.
                if ((curChar != 0) && (textToken_->length > 0)) {
                    closeTextToken();
                    // tokens_.endOfCStr();
                    //  log_w("Text Token: %s", newToken.cStr);
                    // After the following call, newToken may become irrelevant
                    // pageCompleted_ = addTokenToPage(newToken);
                }
            }
        } else if ((curChar == ' ') || (curChar == '\t') || (curChar == '\n')) {
            closeTextToken();
            if (lastToken_ != nullptr) {
                lastToken_->spacesAfter++;
                if (lastToken_->spacesAfter == 1) {
                    pager_.addSpaceWidth();
                }
                // } else {
                // log_e("Lost a space....");
            }
            nextByte();
        } else {
            nextByte();
        }

        if (pageCompleted_) {
            break;
        }
    }

    // if (!pageCompleted_ && tokens_.cStrIsFull()) {
    //     // There is a potential that the last token is not complete (e.g. a TEXT or ENDASH or
    //     // EMDASH), so forget it
    //     tokens_.forgetLast();
    // }

    // REND_WLOG("Computed Word Count: %" PRIu16, pager_.getWordCount());
    // REND_WLOG("Tokens Count: %" PRIu16, tokens_.getTokenCount());

    currentStreamOffset_ = streamOffset + 1;

    if constexpr (REND_DEBUG) {
        if (tokens_.isFull()) {
            REND_ELOG("Tokens_[] is full");
        }
    }

    return !pageCompleted_;
}
