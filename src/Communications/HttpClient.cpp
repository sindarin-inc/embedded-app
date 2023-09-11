#include "HttpClient.hpp"

#include <cstring>
#include <esp_crt_bundle.h>
#include <iostream>
#include <string>

#include "GitVersion.hpp"
#include "Misc/StringUtil.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

HttpClient::HttpClient(const char *url, const char *contentType, esp_http_client_method_t method,
                       bool gzip) {
    url_ = strdup(url);
    userAgent_ = StringFormat("SolReader/%s (%s)", kVersionString, kGitHash);

    esp_http_client_config_t config{
        .url = url_,
        .user_agent = userAgent_.c_str(),
        .method = method,
        .timeout_ms = 30000,
        .event_handler = [](esp_http_client_event_t *event) -> esp_err_t {
            auto thisPtr = (HttpClient *)event->user_data;
            switch (event->event_id) {
            case HTTP_EVENT_ERROR:
                log_e("HTTP_EVENT_ERROR");
                thisPtr->connected = false;
                break;
            case HTTP_EVENT_ON_CONNECTED:
                log_d("HTTP_EVENT_ON_CONNECTED");
                thisPtr->connected = true;
                break;
            case HTTP_EVENT_HEADERS_SENT:
                log_d("HTTP_EVENT_HEADERS_SENT");
                break;
            case HTTP_EVENT_ON_HEADER:
                thisPtr->responseHeaders_.insert(
                    std::pair<std::string, std::string>(event->header_key, event->header_value));
                // log_d("HTTP_EVENT_ON_HEADER %s %s", event->header_key, event->header_value);
                break;
            case HTTP_EVENT_ON_DATA:
                break;
            case HTTP_EVENT_ON_FINISH:
                log_d("HTTP_EVENT_ON_FINISH");
                thisPtr->connected = false;
                break;
            case HTTP_EVENT_DISCONNECTED:
                log_d("HTTP_EVENT_DISCONNECTED");
                thisPtr->connected = false;
                break;
            case HTTP_EVENT_REDIRECT:
                // should not happen, because disable_auto_redirect in the client is not set
                log_d("HTTP_EVENT_REDIRECT");
                break;
            }
            return ESP_OK;
        },
        .user_data = (void *)this,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    client_ = esp_http_client_init(&config);
    if (client_ == nullptr) {
        log_e("Failed esp_http_client_init");
        return;
    }

    if (method != HTTP_METHOD_GET) {
        esp_err_t err = esp_http_client_set_header(client_, "Content-Type", contentType);
        if (err != ESP_OK) {
            log_e("Failed esp_http_client_set_header with %s", esp_err_to_name(err));
        }
    }

    gzip_ = gzip;
    if (gzip_) {
        esp_err_t err = esp_http_client_set_header(client_, "Content-Encoding", "gzip");
        if (err != ESP_OK) {
            log_e("Failed esp_http_client_set_header with %s", esp_err_to_name(err));
        }
    }
}

HttpClient::~HttpClient() {
    esp_http_client_close(client_);
    esp_http_client_cleanup(client_);
    responseHeaders_.clear();
    free(url_);
}

auto HttpClient::setHeader(const char *key, const char *value) -> esp_err_t {
    return esp_http_client_set_header(client_, key, value);
}

auto HttpClient::getResponseHeader(const char *key, char **value) -> esp_err_t {
    auto it = responseHeaders_.find(key);
    if (it == responseHeaders_.end()) {
        return ESP_ERR_NOT_FOUND;
    }
    *value = const_cast<char *>(it->second.c_str());
    return ESP_OK;
}

auto HttpClient::open(int contentLength) -> esp_err_t {
    esp_err_t ret = ESP_OK;
    if (gzip_) {
        gzUncompressedBytesRemaining_ = contentLength;
        gzCompressedLength_ = 0;
        gzipBuffer_ = SpiRamArrayPtr<uint8_t>(gzUncompressedBytesRemaining_);
    } else {
        ret = esp_http_client_open(client_, contentLength);
    }

    return ret;
}

// auto HttpClient::perform() -> esp_err_t { return esp_http_client_perform(client_); }

auto HttpClient::write(const char *body, int length) -> int {
    // check for gzip encoding
    // TODO: check if the stream is already gzipped
    if (gzip_) {

        // initialize gz stream if needed
        if (!gz_.initialized()) {
            gz_.init();
        }

        // write current block of bytes to gzip stream
        if (!gz_.write((uint8_t *)body, length)) {
            log_e("failed to gzip %d bytes", length);
            return -1;
        }

        // decrement written uncompressed bytes so we know when
        // the request is complete and we can send the compressed version.

        // Currently, need to wait until all uncompressed bytes are written first
        // because only then do we know how large the compressed version is.
        gzUncompressedBytesRemaining_ -= length;

        if (gz_.available() > 0) {
            int bytesToRead = gz_.available();

            // read out any available bytes into the preallocated gzip buffer
            if (gz_.read(&gzipBuffer_.get()[gzCompressedLength_], bytesToRead) != bytesToRead) {
                log_e("failed to read gzip data");
                return -1;
            }
            gzCompressedLength_ += bytesToRead;
        }

        // if all uncompressed bytes have been sent, make the request
        // since we now know the size
        if (gzUncompressedBytesRemaining_ == 0) {
            if (!gz_.flush()) {
                log_e("failed to flush gzip stream");
                return -1;
            }

            // read any bytes available after flushing
            int bytesToRead = gz_.available();
            if (gz_.read(&gzipBuffer_.get()[gzCompressedLength_], bytesToRead) != bytesToRead) {
                log_e("failed to read gzip data");
                return -1;
            }
            gzCompressedLength_ += bytesToRead;

            // start http client, using the compressed size
            esp_err_t err = esp_http_client_open(client_, gzCompressedLength_);
            if (err != ESP_OK) {
                log_e("gz could not open http client length %d", gzCompressedLength_);
                return -1;
            }

            // send compressed bytes
            if (esp_http_client_write(client_, (const char *)gzipBuffer_.get(),
                                      gzCompressedLength_) != gzCompressedLength_) {
                log_e("http failed to write %d gz bytes", gzCompressedLength_);
                return -1;
            }
        }

        // return number of bytes consumed
        return length;
    } else {
        return esp_http_client_write(client_, body, length);
    }
}

auto HttpClient::makeWriteStream() -> std::shared_ptr<HttpOstream<4096>> {
    return std::make_shared<HttpOstream<4096>>(client_);
}

auto HttpClient::fetchHeaders() -> int { return esp_http_client_fetch_headers(client_); }

// auto HttpClient::getContentLength() -> int { return esp_http_client_get_content_length(client_);
// }

auto HttpClient::getStatusCode() -> int { return esp_http_client_get_status_code(client_); }

auto HttpClient::read(char *buffer, int length) -> int {
    return esp_http_client_read(client_, buffer, length);
}

auto HttpClient::close() -> esp_err_t { return esp_http_client_close(client_); }

auto HttpClient::makeReadStream() -> std::shared_ptr<HttpIstream<4096>> {
    return std::make_shared<HttpIstream<4096>>(client_);
}
