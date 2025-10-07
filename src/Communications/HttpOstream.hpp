#pragma once

#include <esp_http_client.h>
#include <ostream>
#include <streambuf>

// Written by ChatGPT and untested so far
template <std::size_t BUFFER_SIZE = BUFSIZ>
class HttpOstreambuf : public std::streambuf {
public:
    HttpOstreambuf(std::function<size_t(char *, size_t)> writeFunc)
        : writeFunc_(std::move(writeFunc)) {
        buffer_ = static_cast<char *>(
            heap_caps_malloc(std::max(static_cast<int>(BUFFER_SIZE), BUFSIZ), MALLOC_CAP_SPIRAM));
        setp(buffer_, buffer_ + BUFFER_SIZE - 1);
    }

    ~HttpOstreambuf() override {
        sync();
        SpiramFree(buffer_);
    }

private:
    char *buffer_;
    std::function<size_t(char *, size_t)> writeFunc_;

    auto sync() -> int override {
        int len = pptr() - pbase();
        if (writeFunc_(buffer_, len) != len) {
            return -1;
        }
        setp(buffer_, buffer_ + BUFFER_SIZE - 1);
        return 0;
    }
};

template <std::size_t BUFFER_SIZE = BUFSIZ>
class HttpOstream : public std::ostream {
public:
    HttpOstream(std::function<size_t(char *, size_t)> writeFunc)
        : std::ostream(&buf_), buf_(writeFunc) {}

private:
    HttpOstreambuf<BUFFER_SIZE> buf_;
};
