#pragma once

#include <algorithm>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <functional>
#include <istream>
#include <streambuf>
#include <utility>
#include <vector>

#include "sindarin-debug.h"

template <std::size_t BUFFER_SIZE = BUFSIZ>
class HttpStreambuf : public std::streambuf {
public:
    HttpStreambuf(std::function<size_t(char *, size_t)> readFunc)
        : readFunc_(std::move(std::move(readFunc))) {
        buffer_ = static_cast<char *>(
            heap_caps_malloc(std::max(static_cast<int>(BUFFER_SIZE), BUFSIZ), MALLOC_CAP_SPIRAM));
        setg(buffer_, buffer_, buffer_);
    }

    ~HttpStreambuf() override { heap_caps_free(buffer_); }

private:
    char *buffer_;
    std::function<size_t(char *, size_t)> readFunc_;

    auto underflow() -> int_type override {
        if (gptr() < egptr()) {
            return *gptr();
        }

        char *buffer = eback();
        auto readLength = readFunc_(buffer, BUFSIZ);

        log_w("readLength = %zu", readLength);
        if (readLength <= 0) {
            return traits_type::eof();
        }

        setg(buffer, buffer, buffer + readLength);
        return *gptr();
    }
};

template <std::size_t BUFFER_SIZE = BUFSIZ>
class HttpIstream : public std::istream {
public:
    HttpIstream(std::function<size_t(char *, size_t)> readFunc)
        : std::istream(&buf_), buf_(readFunc) {}

private:
    HttpStreambuf<BUFFER_SIZE> buf_;
};
