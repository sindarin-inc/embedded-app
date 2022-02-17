#include "HtmlCleaner.hpp"

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
    TEXT,
    TAG,
    ELEMENT,
};

void StripHtml(std::stringstream &inStream, std::stringstream &outStream) {
    StripHtmlState state = TEXT;
    std::string element;

    // Running loop till end of stream
    while (true) {
        const char x = inStream.get();
        if (inStream.eof() || !inStream.good()) {
            break;
        }
        if (state == TAG) {
            if (x == '>') {
                state = TEXT;
            }
            continue;
        } else if (state == ELEMENT) {
            element.push_back(x);
            if (x == ';') {
                outStream << ElementFromString(element);
                state = TEXT;
            } else if (x == ' ') {
                // Weird space in there, maybe this wasn't an element
                outStream << element;
                state = TEXT;
            }
            continue;
        }
        // The start of a HTML tag
        if (x == '<') {
            state = TAG;
            continue;
        }
        // The possible star of a HTML Element
        if (x == '&') {
            element = "&";
            state = ELEMENT;
            continue;
        }
        outStream << x;
    }
}

void StripHtml(const std::string &str, std::stringstream &outStream) {
    std::stringstream inStream;
    inStream << str;

    StripHtml(inStream, outStream);
}
