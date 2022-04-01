#include "HtmlCleaner.hpp"

#include <functional>
#include <map>

#include "sindarin-debug.h"

// NOLINTNEXTLINE(cert-err58-cpp)
std::map<std::string, std::string> elementMap = {
    {"&nbsp;", " "},     //
    {"&lt;", "<"},       //
    {"&gt;", ">"},       //
    {"&amp;", "&"},      //
    {"&quot;", "\""},    //
    {"&apos;", "'"},     //
    {"&mdash;", "--"},   //
    {"&ndash;", "-"},    //
    {"&ensp;", " "},     //
    {"&emsp;", "  "},    //
    {"&ldquo;", "\""},   //
    {"&rdquo;", "\""},   //
    {"&lsquo;", "'"},    //
    {"&rsquo;", "'"},    //
    {"&hellip;", "..."}, //
    {"&aring;", "A"},    //
    {"&cent;", "¢"},     //
    {"&pound;", "£"},    //
    {"&yen;", "¥"},      //
    {"&euro;", "€"},     //
    {"&copy;", "©"},     //
    {"&reg;", "®"}       //
};

auto ElementFromString(std::string &str) -> std::string {
    auto search = elementMap.find(str);
    if (search != elementMap.end()) {
        return search->second;
    } else {
        log_w("HTML Element %s not translated", str.c_str());
        return " ";
    }
}

enum StripHtmlState {
    HTML_TEXT,
    HTML_TAG,
    HTML_ELEMENT,
};

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
            // Weird space in there, maybe this wasn't an element
            writeString(element);
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
