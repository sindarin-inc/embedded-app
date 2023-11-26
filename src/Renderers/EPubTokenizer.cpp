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
    {"nbsp",   U'\U000000A0'}, // no-break space = non-breaking space, U+00A0 ISOnum
    {"iexcl",  U'\U000000A1'}, // inverted exclamation mark, U+00A1 ISOnum
    {"cent",   U'\U000000A2'}, // cent sign, U+00A2 ISOnum
    {"pound",  U'\U000000A3'}, // pound sign, U+00A3 ISOnum
    {"curren", U'\U000000A4'}, // currency sign, U+00A4 ISOnum
    {"yen",    U'\U000000A5'}, // yen sign = yuan sign, U+00A5 ISOnum
    {"brvbar", U'\U000000A6'}, // broken bar = broken vertical bar, U+00A6 ISOnum
    {"sect",   U'\U000000A7'}, // section sign, U+00A7 ISOnum
    {"uml",    U'\U000000A8'}, // diaeresis = spacing diaeresis, U+00A8 ISOdia
    {"copy",   U'\U000000A9'}, // copyright sign, U+00A9 ISOnum
    {"ordf",   U'\U000000AA'}, // feminine ordinal indicator, U+00AA ISOnum
    {"laquo",  U'\U000000AB'}, // left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum
    {"not",    U'\U000000AC'}, // not sign = angled dash, U+00AC ISOnum
    {"shy",    U'\U000000AD'}, // soft hyphen = discretionary hyphen, U+00AD ISOnum
    {"reg",    U'\U000000AE'}, // registered sign = registered trade mark sign, U+00AE ISOnum
    {"macr",   U'\U000000AF'}, // macron = spacing macron = overline = APL overbar, U+00AF ISOdia
    {"deg",    U'\U000000B0'}, // degree sign, U+00B0 ISOnum
    {"plusmn", U'\U000000B1'}, // plus-minus sign = plus-or-minus sign, U+00B1 ISOnum
    {"sup2",   U'\U000000B2'}, // superscript two = superscript digit two = squared, U+00B2 ISOnum
    {"sup3",   U'\U000000B3'}, // superscript three = superscript digit three = cubed, U+00B3 ISOnum
    {"acute",  U'\U000000B4'}, // acute accent = spacing acute, U+00B4 ISOdia
    {"micro",  U'\U000000B5'}, // micro sign, U+00B5 ISOnum
    {"para",   U'\U000000B6'}, // pilcrow sign = paragraph sign, U+00B6 ISOnum
    {"middot", U'\U000000B7'}, // middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum
    {"cedil",  U'\U000000B8'}, // cedilla = spacing cedilla, U+00B8 ISOdia
    {"sup1",   U'\U000000B9'}, // superscript one = superscript digit one, U+00B9 ISOnum
    {"ordm",   U'\U000000BA'}, // masculine ordinal indicator, U+00BA ISOnum
    {"raquo",  U'\U000000BB'}, // right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum
    {"frac14", U'\U000000BC'}, // vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum
    {"frac12", U'\U000000BD'}, // vulgar fraction one half = fraction one half, U+00BD ISOnum
    {"frac34", U'\U000000BE'}, // vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum
    {"iquest", U'\U000000BF'}, // inverted question mark = turned question mark, U+00BF ISOnum
    {"Agrave", U'\U000000C0'}, // latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1
    {"Aacute", U'\U000000C1'}, // latin capital letter A with acute, U+00C1 ISOlat1
    {"Acirc",  U'\U000000C2'}, // latin capital letter A with circumflex, U+00C2 ISOlat1
    {"Atilde", U'\U000000C3'}, // latin capital letter A with tilde, U+00C3 ISOlat1
    {"Auml",   U'\U000000C4'}, // latin capital letter A with diaeresis, U+00C4 ISOlat1
    {"Aring",  U'\U000000C5'}, // latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1
    {"AElig",  U'\U000000C6'}, // latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1
    {"Ccedil", U'\U000000C7'}, // latin capital letter C with cedilla, U+00C7 ISOlat1
    {"Egrave", U'\U000000C8'}, // latin capital letter E with grave, U+00C8 ISOlat1
    {"Eacute", U'\U000000C9'}, // latin capital letter E with acute, U+00C9 ISOlat1
    {"Ecirc",  U'\U000000CA'}, // latin capital letter E with circumflex, U+00CA ISOlat1
    {"Euml",   U'\U000000CB'}, // latin capital letter E with diaeresis, U+00CB ISOlat1
    {"Igrave", U'\U000000CC'}, // latin capital letter I with grave, U+00CC ISOlat1
    {"Iacute", U'\U000000CD'}, // latin capital letter I with acute, U+00CD ISOlat1
    {"Icirc",  U'\U000000CE'}, // latin capital letter I with circumflex, U+00CE ISOlat1
    {"Iuml",   U'\U000000CF'}, // latin capital letter I with diaeresis, U+00CF ISOlat1
    {"ETH",    U'\U000000D0'}, // latin capital letter ETH, U+00D0 ISOlat1
    {"Ntilde", U'\U000000D1'}, // latin capital letter N with tilde, U+00D1 ISOlat1
    {"Ograve", U'\U000000D2'}, // latin capital letter O with grave, U+00D2 ISOlat1
    {"Oacute", U'\U000000D3'}, // latin capital letter O with acute, U+00D3 ISOlat1
    {"Ocirc",  U'\U000000D4'}, // latin capital letter O with circumflex, U+00D4 ISOlat1
    {"Otilde", U'\U000000D5'}, // latin capital letter O with tilde, U+00D5 ISOlat1
    {"Ouml",   U'\U000000D6'}, // latin capital letter O with diaeresis, U+00D6 ISOlat1
    {"times",  U'\U000000D7'}, // multiplication sign, U+00D7 ISOnum
    {"Oslash", U'\U000000D8'}, // latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1
    {"Ugrave", U'\U000000D9'}, // latin capital letter U with grave, U+00D9 ISOlat1
    {"Uacute", U'\U000000DA'}, // latin capital letter U with acute, U+00DA ISOlat1
    {"Ucirc",  U'\U000000DB'}, // latin capital letter U with circumflex, U+00DB ISOlat1
    {"Uuml",   U'\U000000DC'}, // latin capital letter U with diaeresis, U+00DC ISOlat1
    {"Yacute", U'\U000000DD'}, // latin capital letter Y with acute, U+00DD ISOlat1
    {"THORN",  U'\U000000DE'}, // latin capital letter THORN, U+00DE ISOlat1
    {"szlig",  U'\U000000DF'}, // latin small letter sharp s = ess-zed, U+00DF ISOlat1
    {"agrave", U'\U000000E0'}, // latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1
    {"aacute", U'\U000000E1'}, // latin small letter a with acute, U+00E1 ISOlat1
    {"acirc",  U'\U000000E2'}, // latin small letter a with circumflex, U+00E2 ISOlat1
    {"atilde", U'\U000000E3'}, // latin small letter a with tilde, U+00E3 ISOlat1
    {"auml",   U'\U000000E4'}, // latin small letter a with diaeresis, U+00E4 ISOlat1
    {"aring",  U'\U000000E5'}, // latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1
    {"aelig",  U'\U000000E6'}, // latin small letter ae = latin small ligature ae, U+00E6 ISOlat1
    {"ccedil", U'\U000000E7'}, // latin small letter c with cedilla, U+00E7 ISOlat1
    {"egrave", U'\U000000E8'}, // latin small letter e with grave, U+00E8 ISOlat1
    {"eacute", U'\U000000E9'}, // latin small letter e with acute, U+00E9 ISOlat1
    {"ecirc",  U'\U000000EA'}, // latin small letter e with circumflex, U+00EA ISOlat1
    {"euml",   U'\U000000EB'}, // latin small letter e with diaeresis, U+00EB ISOlat1
    {"igrave", U'\U000000EC'}, // latin small letter i with grave, U+00EC ISOlat1
    {"iacute", U'\U000000ED'}, // latin small letter i with acute, U+00ED ISOlat1
    {"icirc",  U'\U000000EE'}, // latin small letter i with circumflex, U+00EE ISOlat1
    {"iuml",   U'\U000000EF'}, // latin small letter i with diaeresis, U+00EF ISOlat1
    {"eth",    U'\U000000F0'}, // latin small letter eth, U+00F0 ISOlat1
    {"ntilde", U'\U000000F1'}, // latin small letter n with tilde, U+00F1 ISOlat1
    {"ograve", U'\U000000F2'}, // latin small letter o with grave, U+00F2 ISOlat1
    {"oacute", U'\U000000F3'}, // latin small letter o with acute, U+00F3 ISOlat1
    {"ocirc",  U'\U000000F4'}, // latin small letter o with circumflex, U+00F4 ISOlat1
    {"otilde", U'\U000000F5'}, // latin small letter o with tilde, U+00F5 ISOlat1
    {"ouml",   U'\U000000F6'}, // latin small letter o with diaeresis, U+00F6 ISOlat1
    {"divide", U'\U000000F7'}, // division sign, U+00F7 ISOnum
    {"oslash", U'\U000000F8'}, // latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1
    {"ugrave", U'\U000000F9'}, // latin small letter u with grave, U+00F9 ISOlat1
    {"uacute", U'\U000000FA'}, // latin small letter u with acute, U+00FA ISOlat1
    {"ucirc",  U'\U000000FB'}, // latin small letter u with circumflex, U+00FB ISOlat1
    {"uuml",   U'\U000000FC'}, // latin small letter u with diaeresis, U+00FC ISOlat1
    {"yacute", U'\U000000FD'}, // latin small letter y with acute, U+00FD ISOlat1
    {"thorn",  U'\U000000FE'}, // latin small letter thorn, U+00FE ISOlat1
    {"yuml",   U'\U000000FF'}, // latin small letter y with diaeresis, U+00FF ISOlat1
    {"quot",   U'\U00000022'}, // quotation mark, U+0022 ISOnum
    {"amp",    U'\U00000026'}, // ampersand, U+0026 ISOnum
    {"lt",     U'\U0000003C'}, // less-than sign, U+003C ISOnum
    {"gt",     U'\U0000003E'}, // greater-than sign, U+003E ISOnum
    {"apos",   U'\U00000027'}, // apostrophe = APL quote, U+0027 ISOnum
    {"OElig",  U'\U00000152'}, // latin capital ligature OE, U+0152 ISOlat2
    {"oelig",  U'\U00000153'}, // latin small ligature oe, U+0153 ISOlat2
    {"Scaron", U'\U00000160'}, // latin capital letter S with caron, U+0160 ISOlat2
    {"scaron", U'\U00000161'}, // latin small letter s with caron, U+0161 ISOlat2
    {"Yuml",   U'\U00000178'}, // latin capital letter Y with diaeresis, U+0178 ISOlat2
    {"circ",   U'\U000002C6'}, // modifier letter circumflex accent, U+02C6 ISOpub
    {"tilde",  U'\U000002DC'}, // small tilde, U+02DC ISOdia
    {"ensp",   U'\U00002002'}, // en space, U+2002 ISOpub
    {"emsp",   U'\U00002003'}, // em space, U+2003 ISOpub
    {"thinsp", U'\U00002009'}, // thin space, U+2009 ISOpub
    {"zwnj",   U'\U0000200C'}, // zero width non-joiner, U+200C NEW RFC 2070
    {"zwj",    U'\U0000200D'}, // zero width joiner, U+200D NEW RFC 2070
    {"lrm",    U'\U0000200E'}, // left-to-right mark, U+200E NEW RFC 2070
    {"rlm",    U'\U0000200F'}, // right-to-left mark, U+200F NEW RFC 2070
    {"ndash",  U'\U00002013'}, // en dash, U+2013 ISOpub
    {"mdash",  U'\U00002014'}, // em dash, U+2014 ISOpub
    {"lsquo",  U'\U00002018'}, // left single quotation mark, U+2018 ISOnum
    {"rsquo",  U'\U00002019'}, // right single quotation mark, U+2019 ISOnum
    {"sbquo",  U'\U0000201A'}, // single low-9 quotation mark, U+201A NEW
    {"ldquo",  U'\U0000201C'}, // left double quotation mark,  U+201C ISOnum
    {"rdquo",  U'\U0000201D'}, // right double quotation mark, U+201D ISOnum
    {"bdquo",  U'\U0000201E'}, // double low-9 quotation mark, U+201E NEW
    {"dagger", U'\U00002020'}, // dagger, U+2020 ISOpub
    {"Dagger", U'\U00002021'}, // double dagger, U+2021 ISOpub
    {"permil", U'\U00002030'}, // per mille sign, U+2030 ISOtech
    {"lsaquo", U'\U00002039'}, // single left-pointing angle quotation mark, U+2039 ISO proposed
    {"rsaquo", U'\U0000203A'}, // single right-pointing angle quotation mark, U+203A ISO proposed
    {"euro",   U'\U000020AC'}, // euro sign, U+20AC NEW
};

EPubTokenizer::Characters EPubTokenizer::superscriptCharacters
{
    {"0", U'\U00002070'},
    {"1", U'\U000000B9'},    
    {"2", U'\U000000B2'},    
    {"3", U'\U000000B3'},    
    {"4", U'\U00002074'},
    {"5", U'\U00002075'},
    {"6", U'\U00002076'},
    {"7", U'\U00002077'},
    {"8", U'\U00002078'},
    {"9", U'\U00002079'},

    {"A", U'\U0000E041'}, 
    {"B", U'\U0000E042'}, 
    {"C", U'\U0000E043'}, 
    {"D", U'\U0000E044'}, 
    {"E", U'\U0000E045'}, 
    {"F", U'\U0000E046'}, 
    {"G", U'\U0000E047'}, 
    {"H", U'\U0000E048'}, 
    {"I", U'\U0000E049'}, 
    {"J", U'\U0000E04A'}, 
    {"K", U'\U0000E04B'}, 
    {"L", U'\U0000E04C'}, 
    {"M", U'\U0000E04D'}, 
    {"N", U'\U0000E04E'}, 
    {"O", U'\U0000E04F'}, 
    {"P", U'\U0000E050'}, 
    {"Q", U'\U0000E051'}, 
    {"R", U'\U0000E052'}, 
    {"S", U'\U0000E053'}, 
    {"T", U'\U0000E054'}, 
    {"U", U'\U0000E055'}, 
    {"V", U'\U0000E056'}, 
    {"W", U'\U0000E057'}, 
    {"X", U'\U0000E058'}, 
    {"Y", U'\U0000E059'}, 
    {"Z", U'\U0000E05A'},

    {"a", U'\U0000E05F'}, 
    {"b", U'\U0000E060'}, 
    {"c", U'\U0000E061'}, 
    {"d", U'\U0000E062'}, 
    {"e", U'\U0000E063'}, 
    {"f", U'\U0000E064'}, 
    {"g", U'\U0000E065'}, 
    {"h", U'\U0000E066'}, 
    {"i", U'\U0000E067'}, 
    {"j", U'\U0000E068'}, 
    {"k", U'\U0000E069'}, 
    {"l", U'\U0000E06A'}, 
    {"m", U'\U0000E06B'}, 
    {"n", U'\U0000E06C'}, 
    {"o", U'\U0000E06D'}, 
    {"p", U'\U0000E06E'}, 
    {"q", U'\U0000E06F'}, 
    {"r", U'\U0000E070'}, 
    {"s", U'\U0000E071'}, 
    {"t", U'\U0000E072'}, 
    {"u", U'\U0000E073'}, 
    {"v", U'\U0000E074'}, 
    {"w", U'\U0000E075'}, 
    {"x", U'\U0000E076'}, 
    {"y", U'\U0000E077'}, 
    {"z", U'\U0000E078'}, 

    {"*", U'\U0000E079'},
    {"+", U'\U0000207A'},
    {"-", U'\U0000207B'},
    {"=", U'\U0000207C'},
    {"(", U'\U0000207D'},
    {")", U'\U0000207E'},
    {"[", U'\U0000E07B'},
    {"]", U'\U0000E07C'},
    {",", U'\U0000E05B'},
    {".", U'\U0000E05C'},
    {"/", U'\U0000E05D'},
};

EPubTokenizer::Characters EPubTokenizer::subscriptCharacters
{
    {"0", U'\U00002080'},
    {"1", U'\U00002081'},
    {"2", U'\U00002082'},
    {"3", U'\U00002083'},
    {"4", U'\U00002084'},
    {"5", U'\U00002085'},
    {"6", U'\U00002086'},
    {"7", U'\U00002087'},
    {"8", U'\U00002088'},
    {"9", U'\U00002089'},

    {"A", U'\U0000E005'},
    {"B", U'\U0000E006'},
    {"C", U'\U0000E007'},
    {"D", U'\U0000E008'},
    {"E", U'\U0000E009'},
    {"F", U'\U0000E00A'},
    {"G", U'\U0000E00B'},
    {"H", U'\U0000E00C'},
    {"I", U'\U0000E00D'},
    {"J", U'\U0000E00E'},
    {"K", U'\U0000E00F'},
    {"L", U'\U0000E010'},
    {"M", U'\U0000E011'},
    {"N", U'\U0000E012'},
    {"O", U'\U0000E013'},
    {"P", U'\U0000E014'},
    {"Q", U'\U0000E015'},
    {"R", U'\U0000E016'},
    {"S", U'\U0000E017'},
    {"T", U'\U0000E018'},
    {"U", U'\U0000E019'},
    {"V", U'\U0000E01A'},
    {"W", U'\U0000E01B'},
    {"X", U'\U0000E01C'},
    {"Y", U'\U0000E01D'},
    {"Z", U'\U0000E01E'},

    {"a", U'\U0000E023'},
    {"b", U'\U0000E024'},
    {"c", U'\U0000E025'},
    {"d", U'\U0000E026'},
    {"e", U'\U0000E027'},
    {"f", U'\U0000E028'},
    {"g", U'\U0000E029'},
    {"h", U'\U0000E02A'},
    {"i", U'\U0000E02B'},
    {"j", U'\U0000E02C'},
    {"k", U'\U0000E02D'},
    {"l", U'\U0000E02E'},
    {"m", U'\U0000E02F'},
    {"n", U'\U0000E030'},
    {"o", U'\U0000E031'},
    {"p", U'\U0000E032'},
    {"q", U'\U0000E033'},
    {"r", U'\U0000E034'},
    {"s", U'\U0000E035'},
    {"t", U'\U0000E036'},
    {"u", U'\U0000E037'},
    {"v", U'\U0000E038'},
    {"w", U'\U0000E039'},
    {"x", U'\U0000E03A'},
    {"y", U'\U0000E03B'},
    {"z", U'\U0000E03C'},

    {"*", U'\U0000E03D'},
    {"+", U'\U0000208A'},
    {"-", U'\U0000208B'},
    {"=", U'\U0000208C'},
    {"(", U'\U0000208D'},
    {")", U'\U0000208E'},
    {"[", U'\U0000E03F'},
    {"]", U'\U0000E040'},
    {",", U'\U0000E01F'},
    {".", U'\U0000E020'},
    {"/", U'\U0000E021'},
};

EPubTokenizer::CJKTransList EPubTokenizer::cjkTransList = {
  { U'\U0000FE30', U'\U00002025' },
  { U'\U0000FE31', U'\U00002014' },
  { U'\U0000FE32', U'\U00002013' },
  { U'\U0000FE33', U'\U0000005F' },
  { U'\U0000FE34', U'\U0000005F' },
  { U'\U0000FE35', U'\U00000028' },
  { U'\U0000FE36', U'\U00000029' },
  { U'\U0000FE37', U'\U0000007B' },
  { U'\U0000FE38', U'\U0000007D' },
  { U'\U0000FE39', U'\U00003014' },
  { U'\U0000FE3A', U'\U00003015' },
  { U'\U0000FE3B', U'\U00003010' },
  { U'\U0000FE3C', U'\U00003011' },
  { U'\U0000FE3D', U'\U0000300A' },
  { U'\U0000FE3E', U'\U0000300B' },
  { U'\U0000FE3F', U'\U00003008' },
  { U'\U0000FE40', U'\U00003009' },
  { U'\U0000FE41', U'\U0000300C' },
  { U'\U0000FE42', U'\U0000300D' },
  { U'\U0000FE43', U'\U0000300E' },
  { U'\U0000FE44', U'\U0000300F' },
  { U'\U0000FE45', U'\U0000FE51' },
  { U'\U0000FE47', U'\U0000005B' },
  { U'\U0000FE48', U'\U0000005D' },
  { U'\U0000FE49', U'\U0000203E' },
  { U'\U0000FE4A', U'\U0000203E' },
  { U'\U0000FE4B', U'\U0000203E' },
  { U'\U0000FE4C', U'\U0000203E' },
  { U'\U0000FE4D', U'\U0000005F' },
  { U'\U0000FE4E', U'\U0000005F' },
  { U'\U0000FE4F', U'\U0000005F' },
};

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

// From the string that follow the '&' character, retrieve the entity's UTF32 or the unicode UTF32
// corresponding to the decimal (&#...;) or hexadecimal number (&#x...;). It returns the
// corresponding UTF8 translation and the next position in the input string.
auto EPubTokenizer::getCharEntity(const char *str) -> std::pair<char32_t, const char *> {
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
            return {char32_t(strtol(theStr.c_str(), nullptr, 16)), str};
        } else if (number) {
            return {char32_t(strtol(theStr.c_str(), nullptr, 10)), str};
        } else {
            auto search = entities.find(static_cast<SpiramString>(theStr));
            if (search != entities.end()) {
                return {search->second, str};
            }
        }
    }

    return {U'\U00000000', str};
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
// ----

auto EPubTokenizer::loadAndTokenizePage(StreamPlace fromPlace, bool backwards, StreamPlace *toPlace)
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

    // ----> 4) To have header paragraphs (<h1> .. <h6>) not being orphaned

    startOfHeader_ = false;

    // ----> 5) To have spaces properly added between text tokens (words)

    lastToken_ = nullptr; // To which token spaces will be attached to (as a count)

    // ----> 6) To have supercript/subscript properly processed

    superscript_ = false;
    subscript_ = false;

    // ----> 7) To add brackets around hyperlinks

    preText_[0] = postText_[0] = '\0';

    // ----> 8) To maintain the current bytes offset in the current chapter

    currentTxtOffset_ = 0;
    currentPageTxtOffset_ = 0;

    // ----> 9) To remove paragraph indentation in tables' rows

    inTableRow_ = false;

    // ----> 10) To complete last word token in pcdata

    textToken_ = nullptr;

    // ----> 11) When the page to be displayed is ready

    pageCompleted_ = false;

    // ---

    tokens_.clear();
    pager_.clear();

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
        uint16_t spineIdx = stream_->getSpineIdx(ePubPlace->getHRef());
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
        pictureToken = {.c32Str = nullptr,
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
            display_.height()) {
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
void EPubTokenizer::startNewPageWithToken(Token &token, const char32_t *str, uint8_t strSize) {

    Token tempToken;
    std::u32string tempStr;
    std::shared_ptr<BMPImage> picture = std::move(token.picture);
    if (str == nullptr && (token.length != 0)) {
        tempStr = token.c32Str;
        token.c32Str = nullptr;
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
auto EPubTokenizer::addTokenToPage(Token &token, const char32_t *str, uint8_t strSize) -> bool {
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
            if (((uint16_t)token.tokenId &
                 ((uint16_t)TokenId::NEWLINE | (uint16_t)TokenId::ENDPARAGRAPH)) == 0) {
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
                            newToken = {.c32Str = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::NEWLINE,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, U"\n", 1);
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
                            newToken = {.c32Str = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::NEWLINE,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, U"\n", 1);
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
                    newToken = {.c32Str = nullptr,
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
                    newToken = {.c32Str = nullptr,
                                .picture = nullptr,
                                .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                .pixWidth = 0,
                                .pixHeight = 0,
                                .length = 0,
                                .tokenId = TokenId::BTABLE,
                                .spacesAfter = 0};
                    lastToken_ = nullptr;

                    if (addTokenToPage(newToken, U"{TABLE}", 7)) {
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
                    newToken = {.c32Str = nullptr,
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
                            newToken = {.c32Str = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId =
                                            inTableRow_ ? TokenId::NEWLINE : TokenId::ENDPARAGRAPH,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, U"\n", 1);
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
                            newToken = {.c32Str = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId =
                                            inTableRow_ ? TokenId::NEWLINE : TokenId::ENDPARAGRAPH,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, U"\n", 1);
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
                        if (!beginningOfPage_) {
                            Token &newToken = tokens_.getNextAvailable();
                            newToken = {.c32Str = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::NEWLINE,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, U"\n", 1);
                        }
                        // put a SPACE
                        // if (lastToken_ != nullptr) {
                        //     lastToken_->spacesAfter++;
                        //     if (lastToken_->spacesAfter == 1) {
                        //         pager_.addSpaceWidth();
                        //     }
                        // }
                    }
                    break;

                case Tag::HR:
                    if (tokenizingStarted_) {
                        if (closeTextToken()) {
                            return false;
                        }
                        Token &newToken = tokens_.getNextAvailable();
                        newToken = {.c32Str = nullptr,
                                    .picture = nullptr,
                                    .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                    .pixWidth = 0,
                                    .pixHeight = 0,
                                    .length = 0,
                                    .tokenId = TokenId::HRULE,
                                    .spacesAfter = 0};
                        lastToken_ = nullptr;
                        if (addTokenToPage(newToken, U"\n", 1)) {
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
                            newToken = {.c32Str = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::ENDPARAGRAPH,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, U"\n", 1);
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
                            newToken = {.c32Str = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::ENDPARAGRAPH,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, U"\n", 1);
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
                            newToken = {.c32Str = nullptr,
                                        .picture = nullptr,
                                        .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                        .pixWidth = 0,
                                        .pixHeight = 0,
                                        .length = 0,
                                        .tokenId = TokenId::NEWLINE,
                                        .spacesAfter = 0};
                            lastToken_ = nullptr;

                            addTokenToPage(newToken, U"\n", 1);
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
                        newToken = {.c32Str = nullptr,
                                    .picture = nullptr,
                                    .streamPlace = {currentStreamOffset_, currentSpineIdx_},
                                    .pixWidth = 0,
                                    .pixHeight = 0,
                                    .length = 0,
                                    .tokenId = TokenId::ETABLE,
                                    .spacesAfter = 0};
                        lastToken_ = nullptr;

                        if (addTokenToPage(newToken, U"{END TABLE}", 11)) {
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
    uint32_t lastIncrement = 0;

    // REND_WLOG("Started at offset: %" PRIu32, streamOffset);

    char32_t curChar;

    // Keep it DRY

    auto nextChar = [&curChar, &str, &len, &streamOffset, &lastIncrement, this]() -> void {
        if (len == 0) {
            curChar = 0;
        } else {
            uint8_t c = str[0];
            auto size = (c & 0x80) == 0
                            ? 1
                            : (((c & 0xE0) == 0xC0)
                                   ? 2
                                   : (((c & 0xF0) == 0xE0) ? 3 : (((c & 0xF8) == 0xF0) ? 4 : 0)));
            if ((size != 0) && (len >= size)) {
                switch (size) {
                case 1:
                    curChar = c;
                    break;
                case 2:
                    curChar = ((c & 0x1F) << 6) + (str[1] & 0x3F);
                    break;
                case 3:
                    curChar = ((c & 0x0F) << 12) + ((str[1] & 0x3F) << 6) + (str[2] & 0x3F);
                    break;
                case 4:
                    curChar = ((c & 0x07) << 18) + ((str[1] & 0x3F) << 12) +
                              ((str[2] & 0x3F) << 6) + (str[3] & 0x3F);
                    break;
                }
                str += size;
                len -= size;
                streamOffset += lastIncrement;
                lastIncrement = size;
                if (backwards_) {
                    currentTxtOffset_ += size;
                    currentPageTxtOffset_ += size;
                }

                auto trans = cjkTransList.find(curChar);
                if (trans != cjkTransList.end()) {
                    curChar = trans->second;
                }
            } else {
                curChar = 0;
            }
        }
    };

    nextChar();

    if (beginningOfPage_) {
        while ((curChar != 0) && isspace(curChar)) {
            nextChar();
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

        if (superscript_) {
            superscript_ = false;
            Token &newToken = tokens_.getNextAvailable();
            newToken = {.c32Str = nullptr,
                        .picture = nullptr,
                        .streamPlace = {streamOffset, currentSpineIdx_},
                        .pixWidth = 0,
                        .pixHeight = 0,
                        .length = 0,
                        .tokenId = TokenId::TEXT,
                        .spacesAfter = 0};
            do {
                auto chIt = superscriptCharacters.find(SpiramString((char *)&curChar, 1));
                if (chIt != superscriptCharacters.end()) {
                    const char32_t c32 = chIt->second;
                    if (!tokens_.cStrIsFull()) {
                        tokens_.addChar(newToken, c32);
                    } else {
                        break;
                    }
                }
                nextChar();
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
            newToken = {.c32Str = nullptr,
                        .picture = nullptr,
                        .streamPlace = {streamOffset, currentSpineIdx_},
                        .pixWidth = 0,
                        .pixHeight = 0,
                        .length = 0,
                        .tokenId = TokenId::TEXT,
                        .spacesAfter = 0};
            do {
                auto chIt = subscriptCharacters.find(SpiramString((char *)&curChar, 1));
                if (chIt != subscriptCharacters.end()) {
                    const char32_t c32 = chIt->second;
                    if (!tokens_.cStrIsFull()) {
                        tokens_.addChar(newToken, c32);
                    } else {
                        break;
                    }
                }
                nextChar();
            } while (curChar != 0);

            if (newToken.length > 0) {
                tokens_.endOfCStr();
                lastToken_ = &newToken; // Must be done before calling addTokenToPage()

                // After the following call, newToken may become irrelevant
                addTokenToPage(newToken);
            }
        } else if (curChar > U' ') {
            if (textToken_ == nullptr) {
                textToken_ = &tokens_.getNextAvailable();

                *textToken_ = {.c32Str = nullptr,
                               .picture = nullptr,
                               .streamPlace = {streamOffset, currentSpineIdx_},
                               .pixWidth = 0,
                               .pixHeight = 0,
                               .length = 0,
                               .tokenId = TokenId::TEXT,
                               .spacesAfter = 0};
                textCharCount_ = 0;
            }

            if ((textToken_->length == 0) && (preText_[0] != U'\0')) {
                tokens_.addChars(*textToken_, preText_);
                preText_[0] = U'\0';
            }

            if (curChar == U'-') {
                textToken_->tokenId = startOfHeader_ ? TokenId::HTEXT : TokenId::HYPHEN;
                startOfHeader_ = false;
                tokens_.addChar(*textToken_, U'-');
                nextChar();
                closeTextToken();
            } else {
                if (startOfHeader_) {
                    textToken_->tokenId = TokenId::HTEXT;
                    startOfHeader_ = false;
                }

                do {
                    // log_d("Current Character: %c (%02" PRIX8 "H)",
                    //       (((curChar >= ' ') && (curChar < 127)) ? curChar : ' '),
                    //       curChar);

                    if (curChar == U'&') {
                        auto [trans, newStrLocation] = getCharEntity(str);
                        if (trans != 0) {
                            len -= (newStrLocation - str);
                            streamOffset += (newStrLocation - str);
                            if (!tokens_.cStrIsFull() && (trans > U' ')) {
                                tokens_.addChar(*textToken_, trans);
                            }

                            textCharCount_ += 1;
                            str = newStrLocation;
                            nextChar();
                        } else {
                            if (!tokens_.cStrIsFull()) {
                                tokens_.addChar(*textToken_, curChar);
                                textCharCount_ += 1;
                            } else {
                                break;
                            }
                            nextChar();
                        }
                    } else {
                        textCharCount_ += 1;
                        if (!tokens_.cStrIsFull()) {
                            // if (curChar == 0xC2U) {
                            //     nextChar();
                            //     if (curChar == 0xA0U) {
                            //         curChar = ' ';
                            //     } else {
                            //         tokens_.addChar(newToken, 0xC2U);
                            //     }
                            // }
                            tokens_.addChar(*textToken_, curChar);
                        } else {
                            break;
                        }
                        if (curChar == U'-') {
                            nextChar();
                            break;
                        }
                        nextChar();
                    }
                } while ((curChar > U' ') && ((textCharCount_ < maxTextTokenCharSize_)));

                // if we reach the end of the pcdata, we keep the textToken open. It will be
                // completed with other pcdata, or by the next tag that is known to require that
                // all remainig text was processed. This is the case of all the tags that are
                // considered by the tokenizer.
                if ((curChar != 0) && (textToken_->length > 0)) {
                    closeTextToken();
                    // tokens_.endOfCStr();
                    //  log_w("Text Token: %s", newToken.c32Str);
                    // After the following call, newToken may become irrelevant
                    // pageCompleted_ = addTokenToPage(newToken);
                }
            }
        } else if ((curChar == U' ') || (curChar == U'\t')) {
            closeTextToken();
            if (lastToken_ != nullptr) {
                lastToken_->spacesAfter++;
                if (lastToken_->spacesAfter == 1) {
                    pager_.addSpaceWidth();
                }
                // } else {
                // log_e("Lost a space....");
            }
            nextChar();
        } else {
            nextChar();
        }

        if (pageCompleted_) {
            break;
        }
    }

    // if (!pageCompleted_ && tokens_.c32StrIsFull()) {
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
