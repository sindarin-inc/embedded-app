#include <esp_http_client.h>
#include <istream>
#include <streambuf>

#include "sindarin-debug.h"

template <std::size_t BUFFER_SIZE = BUFSIZ>
class HttpStreambuf : public std::streambuf {
public:
    HttpStreambuf(esp_http_client_handle_t client) : client_(client) {
        buffer_ = (char *)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        setg(buffer_, buffer_, buffer_);
    }

    ~HttpStreambuf() override { heap_caps_free(buffer_); }

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
