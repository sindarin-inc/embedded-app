#include "WiFiHttpClient.hpp"

#if CONFIG_USE_WIFI

#include <StringUtil/StringUtil.hpp>
#include <cstring>
#include <esp_crt_bundle.h>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Communications/HttpClient.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

WiFiHttpClient::WiFiHttpClient(const char *url, const char *contentType,
                               esp_http_client_method_t method, bool gzip, int timeoutMs)
    : HttpClient(url, contentType, method, gzip) {

    userAgent_ = StringFormat("SolReader/%s (%s)", SemanticVersionString(), GitVersionString());

    esp_http_client_config_t config{
        .url = url_,
        .user_agent = userAgent_.c_str(),
        .method = method,
        .timeout_ms = timeoutMs,
        .event_handler = [](esp_http_client_event_t *event) -> esp_err_t {
            auto thisPtr = static_cast<WiFiHttpClient *>(event->user_data);
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
        .user_data = reinterpret_cast<void *>(this),
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
            log_e("esp_http_client_set_header err=%s", esp_err_to_name(err));
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

WiFiHttpClient::~WiFiHttpClient() {
    // In case the connection wasn't already closed
    esp_http_client_close(client_);
    esp_http_client_cleanup(client_);
    free(url_);
}

auto WiFiHttpClient::setHeader(const char *key, const char *value) -> esp_err_t {
    auto err = esp_http_client_set_header(client_, key, value);
    if (err != ESP_OK) {
        log_e("esp_http_client_set_header err=%s", esp_err_to_name(err));
    }
    return err;
}

auto WiFiHttpClient::open(int contentLength) -> esp_err_t {
    esp_err_t ret = ESP_OK;

    if (gzip_) {
        // use chunked transfer for gzip requests,
        // since we do not know the content length until after compression.
        gzUncompressedBytesRemaining_ = contentLength;
        ret = esp_http_client_open(client_, -1);
    } else {
        ret = esp_http_client_open(client_, contentLength);
    }

    if (ret != ESP_OK) {
        log_e("esp_http_client_open err=%s", esp_err_to_name(ret));
    }

    return ret;
}

// auto WiFiHttpClient::perform() -> esp_err_t { return esp_http_client_perform(client_); }

auto WiFiHttpClient::write(const char *body, int length) -> int {
    if (gzip_) {
        // initialize gz stream if needed
        if (!gz_.initialized()) {
            gz_.init();
        }

        // write current block of bytes to gzip stream
        if (!gz_.write(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(body)), length)) {
            log_e("failed to gzip %d bytes", length);
            return -1;
        }

        gzUncompressedBytesRemaining_ -= length;
        if (!writeAvailableCompressedData()) {
            return -1;
        }

        if (gzUncompressedBytesRemaining_ == 0) {
            if (!gz_.flush()) {
                log_e("failed to flush gz stream");
                return -1;
            }

            if (!writeAvailableCompressedData()) {
                return -1;
            }

            // write empty chunk to end the chunked encoding
            if (!endChunkedData()) {
                return -1;
            }
        }

        // return number of bytes consumed
        return length;
    } else {
        return esp_http_client_write(client_, body, length);
    }
}

auto WiFiHttpClient::writeAvailableCompressedData() -> bool {
    int avail = gz_.available();
    if (avail > 0) {
        if (!writeChunkedData(reinterpret_cast<const char *>(gz_.getDataPtr()), avail)) {
            return false;
        }
        gz_.advance(avail);
    }
    return true;
}

auto WiFiHttpClient::isChunkedResponse() -> bool {
    return esp_http_client_is_chunked_response(client_);
}

auto WiFiHttpClient::writeChunkedData(const char *data, int length) -> bool {
    char lstr[32];
    if (!SafeSnprintf(lstr, sizeof(lstr), "%x\r\n", length)) {
        return false;
    }

    if (esp_http_client_write(client_, lstr, strlen(lstr)) != strlen(lstr)) {
        log_e("http failed to write %zu bytes", strlen(lstr));
        return false;
    }

    if (length > 0) {
        if (esp_http_client_write(client_, data, length) != length) {
            log_e("http failed to write %d gz bytes", length);
            return false;
        }
    }

    if (!SafeSnprintf(lstr, sizeof(lstr), "\r\n")) {
        return false;
    }

    if (esp_http_client_write(client_, lstr, strlen(lstr)) != strlen(lstr)) {
        log_e("http failed to write %zu bytes", strlen(lstr));
        return false;
    }

    return true;
}

auto WiFiHttpClient::endChunkedData() -> bool { return writeChunkedData(nullptr, 0); }

auto WiFiHttpClient::fetchHeaders(const std::shared_ptr<CancelFlag> &cancel) -> int {
    return esp_http_client_fetch_headers(client_);
}

// auto WiFiHttpClient::getContentLength() -> int { return
// esp_http_client_get_content_length(client_);
// }

auto WiFiHttpClient::getStatusCode() -> int { return esp_http_client_get_status_code(client_); }

auto WiFiHttpClient::read(char *buffer, int length) -> int {
    return esp_http_client_read(client_, buffer, length);
}

auto WiFiHttpClient::close() -> esp_err_t {
    auto err = esp_http_client_close(client_);
    if (err != ESP_OK) {
        log_e("esp_http_client_close err=%s", esp_err_to_name(err));
    }
    return err;
}

#endif
