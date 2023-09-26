#include "HttpClient.hpp"

#include <cstring>
#include <esp_crt_bundle.h>
#include <iostream>
#include <string>

#include "GitVersion.hpp"
#include "Misc/StringUtil.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

HttpClient::HttpClient(const char *url, const char *contentType, esp_http_client_method_t method) {
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
            log_e("esp_http_client_set_header err=%s", esp_err_to_name(err));
        }
    }
}

HttpClient::~HttpClient() {
    // In case the connection wasn't already closed
    esp_http_client_close(client_);
    esp_http_client_cleanup(client_);
    free(url_);
}

auto HttpClient::setHeader(const char *key, const char *value) -> esp_err_t {
    auto err = esp_http_client_set_header(client_, key, value);
    if (err != ESP_OK) {
        log_e("esp_http_client_set_header err=%s", esp_err_to_name(err));
    }
    return err;
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
    auto err = esp_http_client_open(client_, contentLength);
    if (err != ESP_OK) {
        log_e("esp_http_client_open err=%s", esp_err_to_name(err));
    }
    return err;
}

// auto HttpClient::perform() -> esp_err_t { return esp_http_client_perform(client_); }

auto HttpClient::write(const char *body, int length) -> int {
    return esp_http_client_write(client_, body, length);
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

auto HttpClient::close() -> esp_err_t {
    auto err = esp_http_client_close(client_);
    if (err != ESP_OK) {
        log_e("esp_http_client_close err=%s", esp_err_to_name(err));
    }
    return err;
}

auto HttpClient::makeReadStream() -> std::shared_ptr<HttpIstream<4096>> {
    return std::make_shared<HttpIstream<4096>>(client_);
}
