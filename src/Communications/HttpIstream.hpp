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
class HttpIStreambuf : public std::streambuf {
public:
    HttpIStreambuf(std::function<size_t(char *, size_t)> readFunc)
        : readFunc_(std::move(readFunc)) {
        setg(buffer_, buffer_, buffer_);
    }

private:
    char buffer_[BUFFER_SIZE] = {};
    std::function<size_t(char *, size_t)> readFunc_;

    auto underflow() -> int_type override {
        auto readLength = readFunc_(buffer_, BUFFER_SIZE);
        if (readLength <= 0) {
            return traits_type::eof();
        }

        setg(buffer_, buffer_, buffer_ + readLength);

        return *gptr();
    }
};

template <std::size_t BUFFER_SIZE = BUFSIZ>
class HttpIstream : public std::istream {
public:
    HttpIstream(std::function<size_t(char *, size_t)> readFunc)
        : std::istream(&buf_), buf_(readFunc) {}

private:
    HttpIStreambuf<BUFFER_SIZE> buf_;
};
