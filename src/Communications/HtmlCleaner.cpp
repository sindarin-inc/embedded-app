#include "HtmlCleaner.hpp"

#include <functional>
#include <map>

#include "sindarin-debug.h"

// NOLINTNEXTLINE(cert-err58-cpp)
std::map<std::string, std::string> elementMap = {
    /* clang-format off */
    {"&nbsp;", " "},
    {"&lt;", "<"},
    {"&gt;", ">"},
    {"&amp;", "&"},
    {"&quot;", "\""},
    {"&apos;", "'"},
    {"&mdash;", "--"},
    {"&ndash;", "-"},
    {"&ensp;", " "},
    {"&emsp;", "  "},
    {"&ldquo;", "\""},
    {"&rdquo;", "\""},
    {"&lsquo;", "'"},
    {"&rsquo;", "'"},
    {"&hellip;", "..."},
    {"&aring;", "A"},
    {"&cent;", "¢"},
    {"&pound;", "£"},
    {"&yen;", "¥"},
    {"&euro;", "€"},
    {"&copy;", "©"},
    {"&reg;", "®"},
    // TODO: Do Unicode for real
    {"\xE2\x80\x9C", "\""}, // Open Quotes “
    {"\xE2\x80\x9D", "\""}, // Close Quotes ”
    {"\xE2\x80\x99", "'"},  // Apostrophe ’
    {"\xE2\x80\x94", "-"},  // —
    {"\xE2\x80\xA6", "..."}, // …
    {"\xE2\x80\x98", "'"}   // ‘
    /* clang-format: on */
};

auto ElementFromString(std::string &str) -> std::string {
    auto search = elementMap.find(str);
    if (search != elementMap.end()) {
        return search->second;
    } else {
        log_w("Element %s not translated", str.c_str());
        return " ";
    }
}

auto UTF8ElementFromString(std::string &str) -> std::string {
    auto search = elementMap.find(str);
    if (search != elementMap.end()) {
        return search->second;
    } else {
        log_w("Element %s not translated", str.c_str());
        return " ";
    }
}

void AddChar(StripHtmlState &state, std::string &element, const char x,
             const std::function<void(std::string)> &writeString) {
    if (state == HTML_TAG) {
        if (x == '>') {
            state = HTML_TEXT;
        }
        return;
    } else if (state == HTML_ELEMENT) {
        element.push_back(x);
        if (x == ';') {
            writeString(ElementFromString(element));
            state = HTML_TEXT;
        } else if (x == ' ') {
            // Weird space in there, maybe this wasn't an element writeString(element);
            state = HTML_TEXT;
        }
        return;
    } else if (state == UTF8_ELEMENT) {
        element.push_back(x);
        if (element.length() == 3) {
            writeString(ElementFromString(element));
            state = HTML_TEXT;
        }
        return;
    }
    // The start of a HTML tag
    if (x == '<') {
        state = HTML_TAG;
        return;
    }
    // The possible star of a HTML Element
    if (x == '&') {
        element = "&";
        state = HTML_ELEMENT;
        return;
    }
    if (x == '\xE2') {
        element = "\xE2";
        state = UTF8_ELEMENT;
        return;
    }

    writeString(std::string(1, x));
}

void StripHtml(std::stringstream &inStream, std::stringstream &outStream) {
    StripHtmlState state = HTML_TEXT;
    std::string element;

    // Running loop till end of stream
    while (true) {
        char x;
        inStream.get(x);
        if (inStream.eof() || !inStream.good()) {
            break;
        }
        AddChar(state, element, x, [&outStream](const std::string &x) { outStream << x; });
    }
}

void StripHtml(const std::string &str, std::stringstream &outStream) {
    std::stringstream inStream;
    inStream << str;

    StripHtml(inStream, outStream);
}

void StripHtml(Stream &inStream, std::stringstream &outStream) {
    StripHtmlState state = HTML_TEXT;
    std::string element;

    // Running loop till end of stream
    while (inStream.available()) {
        const char x = inStream.read();
        AddChar(state, element, x, [&outStream](const std::string &x) { outStream << x; });
    }
}

void StripHtml(std::stringstream &inStream, Stream &outStream) {
    StripHtmlState state = HTML_TEXT;
    std::string element;

    // Running loop till end of stream
    while (true) {
        char x;
        inStream.get(x);
        if (inStream.eof() || !inStream.good()) {
            break;
        }
        AddChar(state, element, x,
                [&outStream](const std::string &x) { outStream.write(x.c_str()); });
    }
}

void StripHtml(const std::string &str, Stream &outStream) {
    std::stringstream inStream;
    inStream << str;

    StripHtml(inStream, outStream);
}

void StripHtml(const std::basic_string<char, std::char_traits<char>, SpiramAllocator<char>> &str,
               Stream &outStream) {
    std::stringstream inStream;
    inStream << str;

    StripHtml(inStream, outStream);
}

// TODO: There has got to be a better way
auto BytesAvailable(const std::stringstream &s) -> std::size_t {
    std::streambuf *buf = s.rdbuf();
    std::streampos pos = buf->pubseekoff(0, std::ios_base::cur, std::ios_base::in);
    std::streampos end = buf->pubseekoff(0, std::ios_base::end, std::ios_base::in);
    buf->pubseekpos(pos, std::ios_base::in);
    return end - pos;
}

auto HtmlCleaner::read() -> int {
    // Read from the source stream until we get a character
    while (BytesAvailable(outStream_) <= 0 && stream_->available()) {
        const char x = stream_->read();
        AddChar(state_, element_, x, [this](const std::string &x) { outStream_ << x; });
    }

    position_ += 1;
    return outStream_.get();
};
