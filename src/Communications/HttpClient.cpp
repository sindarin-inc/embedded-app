
#include "Communications/HttpClient.hpp"

HttpClient::HttpClient(const char *url, const char *contentType, esp_http_client_method_t method,
                       bool gzip)
    : url_(strdup(url)) {}

HttpClient::~HttpClient() = default;
