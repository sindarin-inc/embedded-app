#pragma once

#include <algorithm>
#include <esp_http_client.h>
#include <istream>
#include <streambuf>

#include "Misc/SpiramAllocator.hpp"
#include "sindarin-debug.h"

template <std::size_t BUFFER_SIZE = BUFSIZ>
class HttpStreambuf : public std::streambuf {
public:
    HttpStreambuf(esp_http_client_handle_t client) : client_(client) {
        buffer_ =
            static_cast<char *>(SpiRamMalloc(std::max(static_cast<int>(BUFFER_SIZE), BUFSIZ)));
        setg(buffer_, buffer_, buffer_);
    }

    ~HttpStreambuf() override { SpiRamFree(buffer_); }

private:
    char *buffer_;
    esp_http_client_handle_t client_;

    auto underflow() -> int_type override {
        if (gptr() < egptr()) {
            return *gptr();
        }

        char *buffer = eback();
        int readLength = esp_http_client_read(client_, buffer, BUFSIZ);

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
    HttpIstream(esp_http_client_handle_t client) : std::istream(&buf_), buf_(client) {}

private:
    HttpStreambuf<BUFFER_SIZE> buf_;
};
