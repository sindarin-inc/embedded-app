#ifndef _STRING_STREAM_H_
#define _STRING_STREAM_H_

#include <Stream.h>
#include <iostream>
#include <sstream>
#include <string>

class StringStream : public Stream {
public:
    StringStream(std::stringstream &stream) : stream_(std::move(stream)) {}
    StringStream(std::string &string) : stream_(string) { setSize(string.length()); }

    // StringStream(String &s) : string(s), position(0) {}

    void setSize(size_t size) { size_ = size; }

    // Stream methods

    auto available() -> int override { return stream_.peek() != EOF; }
    auto read() -> int override { return stream_.get(); };
    // auto seek(uint32_t pos) -> bool override {
    //     stream_.seekg(pos);
    //     return stream_.good();
    // };
    auto position() -> size_t { return stream_.tellg(); }
    auto size() -> size_t { return size_; }
    // virtual int peek() { return position < string.length() ? string[position] : -1; }

    // virtual int available() { return string.length() - position; }
    // virtual int read() { return position < string.length() ? string[position++] : -1; }
    virtual int peek() override { return 0; }
    virtual void flush() override{};
    // Print methods
    virtual size_t write(uint8_t c) {
        // string += (char)c;
        return 1;
    };

private:
    std::stringstream stream_;
    size_t size_ = 0;
    unsigned int length;
    // unsigned int position;
};

#endif // _STRING_STREAM_H_

// class StdStringStream : public RendererStream {
// public:
//     StdStringStream(std::stringstream &stream) : stream_(std::move(stream)) {}
//     StdStringStream(std::string &string) : stream_(string) { setSize(string.length()); }

//     void setSize(size_t size) { size_ = size; }

//     auto available() -> int override { return stream_.peek() != EOF; }
//     auto read() -> int override { return stream_.get(); };
//     auto seek(uint32_t pos) -> bool override {
//         stream_.seekg(pos);
//         return stream_.good();
//     };
//     auto position() -> size_t override { return stream_.tellg(); }
//     auto size() -> size_t override { return size_; }

// protected:
//     std::stringstream stream_;
//     size_t size_ = 0;
// };
