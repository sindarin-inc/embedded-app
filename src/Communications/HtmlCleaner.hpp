#pragma once

#include <Stream.h>
#include <iostream>
#include <sstream>
#include <string>

#include "Misc/SpiramAllocator.hpp"
#include "Renderers/RendererStream.hpp"

enum StripHtmlState {
    HTML_TEXT,
    HTML_TAG,
    HTML_ELEMENT,
    UTF8_ELEMENT,
};

// TODO: A Stream<->std::stringstream adapter class could simplify this
void StripHtml(std::stringstream &inStream, std::stringstream &outStream);
void StripHtml(const std::string &str, std::stringstream &outStream);
void StripHtml(Stream &inStream, std::stringstream &outStream);
void StripHtml(std::stringstream &inStream, Stream &outStream);
void StripHtml(const std::string &str, Stream &outStream);
void StripHtml(const std::basic_string<char, std::char_traits<char>, SpiramAllocator<char>> &str,
               Stream &outStream);

class HtmlCleaner : public RendererStream {
public:
    HtmlCleaner(std::unique_ptr<RendererStream> stream) : stream_(std::move(stream)) {}

    // We can't know precisely how many bytes are available here since we don't know the size() of
    // the stripped content
    auto available() -> int override { return stream_->available() > 0; };
    auto read() -> int override;
    auto seek(uint32_t pos) -> bool override {
        if (pos < position_) {
            log_w("Seeking backwards in HtmlCleaner from %d to %d", position_, pos);
            position_ = 0;
            outStream_.str(std::string());
            outStream_.clear();
            element_ = "";
            state_ = HTML_TEXT;
            stream_->seek(0);
        }

        // Seek forward to position
        while (position_ < pos) {
            // Read a byte at a time until we get to the position
            read();
        }
        return true;
    };
    auto position() -> size_t override { return position_; };
    auto size() -> size_t override {
        // log_w("HtmlCleaner is undetermined until the whole stream is read");
        return stream_->size();
    };

private:
    std::unique_ptr<RendererStream> stream_;
    StripHtmlState state_{HTML_TEXT};
    std::string element_;
    std::stringstream outStream_;

    int position_{0};
};
