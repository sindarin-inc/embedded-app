#pragma once

#include <esp_http_client.h>
#include <ostream>
#include <streambuf>

// Written by ChatGPT and untested so far
template <std::size_t BUFFER_SIZE = BUFSIZ>
class HttpOstreambuf : public std::streambuf {
public:
    HttpOstreambuf(esp_http_client_handle_t client) : client_(client) {
        buffer_ = (char *)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        setp(buffer_, buffer_ + BUFFER_SIZE - 1);
    }

    ~HttpOstreambuf() override {
        sync();
        heap_caps_free(buffer_);
    }

private:
    esp_http_client_handle_t client_;
    char *buffer_;

    auto sync() -> int override {
        int len = pptr() - pbase();
        if (esp_http_client_write(client_, buffer_, len) != len) {
            return -1;
        }
        setp(buffer_, buffer_ + BUFFER_SIZE - 1);
        return 0;
    }
};

template <std::size_t BUFFER_SIZE = BUFSIZ>
class HttpOstream : public std::ostream {
public:
    HttpOstream(esp_http_client_handle_t client) : std::ostream(&buf_), buf_(client) {}

private:
    HttpOstreambuf<BUFFER_SIZE> buf_;
};
