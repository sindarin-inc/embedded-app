#include "esp_http_client.h"

#include "sindarin-debug.h"

// Ignore warnings about deprecated OpenSSL stuff
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <httplib.h>
#pragma clang diagnostic pop

class FakeClient {
public:
    FakeClient(const esp_http_client_config_t *config) : config(*config) {
        // yhirose/cpp-httplib does not want to do URL parsing so we must do it ourselves
        // See: https://github.com/yhirose/cpp-httplib/issues/453
        std::string urlStr(config->url);
        size_t found = urlStr.find_first_of(":");
        std::string protocol = urlStr.substr(0, found);
        // urlNew is the url excluding the http part
        std::string urlNew = urlStr.substr(found + 3);
        size_t found1 = urlNew.find_first_of(":");
        size_t found2 = urlNew.find_first_of("/");

        host = urlStr.substr(0, found2 + protocol.length() + 3);
        path = urlNew.substr(found2);

        client = new httplib::Client(host);
        if (!client->is_valid()) {
            log_e("Invalid client");
        }
    }

    ~FakeClient() {
        delete client;
        if (stream) {
            delete stream;
        }
    }

    esp_http_client_config_t config;

    httplib::Client *client;

    // Request things
    std::string host;
    std::string path;
    std::string contentType;
    std::string writeBody;
    httplib::Headers headers;

    // Response things
    std::stringstream *stream;
    int status;
    int contentLength;
    httplib::Headers responseHeaders;

    void makeRequestIfNecessary() {
        if (requestMade_) {
            return;
        }
        requestMade_ = true;

        auto result = makeRequest();

        if (!result) {
            log_e("httplib: No response");
            return;
        }

        status = result->status;
        contentLength = result->body.length();
        responseHeaders = result->headers;

        // log_d("Request headers:");
        // for (auto it = headers.begin(); it != headers.end(); it = headers.upper_bound(it->first))
        // {
        //     std::cout << "  " << it->first << ": " << it->second << std::endl;
        // }

        // log_d("Response headers");
        for (auto it = responseHeaders.begin(); it != responseHeaders.end();
             it = responseHeaders.upper_bound(it->first)) {
            // std::cout << "  " << it->first << ": " << it->second << std::endl;
            esp_http_client_event_t event;
            event.user_data = config.user_data;
            event.event_id = HTTP_EVENT_ON_HEADER;
            event.header_key = const_cast<char *>(it->first.c_str());
            event.header_value = const_cast<char *>(it->second.c_str());
            config.event_handler(&event);
        }
        log_d("httplib: Got response w/ size=%zu", result->body.length());

        // You might think to yourself: "Why not just put a std::stringstream on the class instead
        // of using new". Well that's what I used to think as well. But there was some mysterious
        // issue where writing to the stream (`stream << result->body`) would truncate the string
        // without throwing any sort of error. This could either be something that limited the copy
        // size of std::string (I was trying to copy 300k) OR it could be something in the
        // std::stringstream that limited the bytes I could write to it.
        stream = new std::stringstream(std::move(result->body));
    }

protected:
    bool requestMade_ = false;

    auto makeRequest() -> httplib::Result {
        client->set_decompress(false);
        if (config.method == HTTP_METHOD_POST) {
            return client->Post(path.c_str(), headers, writeBody, contentType.c_str());
        } else { // config.method == HTTP_METHOD_GET
            return client->Get(path.c_str(), headers);
        }
    }
};

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config) {
    FakeClient *fakeClient = new FakeClient(config);
    return fakeClient;
}

// esp_err_t esp_http_client_perform(esp_http_client_handle_t client) {}

// esp_err_t esp_http_client_set_url(esp_http_client_handle_t client, const char *url) {}

// esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char *data,
//                                          int len) {}

// int esp_http_client_get_post_field(esp_http_client_handle_t client, char **data) {}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key,
                                     const char *value) {
    if (std::string(key) == "Content-Type") {
        // Special handling for Content-Type because that's what httplib wants
        client->contentType = value;
    } else {
        client->headers.insert(std::pair<std::string, std::string>(key, value));
    }
    return ESP_OK;
}

esp_err_t esp_http_client_get_header(esp_http_client_handle_t client, const char *key,
                                     char **value) {
    // Grab the header from the request headers std::multimap
    auto it = client->headers.find(key);
    if (it == client->headers.end()) {
        return ESP_ERR_NOT_FOUND;
    }

    *value = const_cast<char *>(it->second.c_str());
    return ESP_OK;
}

// esp_err_t esp_http_client_get_username(esp_http_client_handle_t client, char **value) {}

// esp_err_t esp_http_client_set_username(esp_http_client_handle_t client, const char *username)
// {}

// esp_err_t esp_http_client_get_password(esp_http_client_handle_t client, char **value) {}

// esp_err_t esp_http_client_set_password(esp_http_client_handle_t client, const char *password)
// {}

// esp_err_t esp_http_client_set_authtype(esp_http_client_handle_t client,
//    esp_http_client_auth_type_t auth_type) {}

// int esp_http_client_get_errno(esp_http_client_handle_t client) {}

// esp_err_t esp_http_client_set_method(esp_http_client_handle_t client,
//                                      esp_http_client_method_t method) {}

// esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t client, int timeout_ms) {}

// esp_err_t esp_http_client_delete_header(esp_http_client_handle_t client, const char *key) {}

esp_err_t esp_http_client_open(esp_http_client_handle_t client, int write_len) {
    client->client->set_follow_location(true);
    client->client->set_logger([client](const httplib::Request &req, const httplib::Response &res) {
        log_d("httpclient: %s %s%s (status=%d)", req.method.c_str(), client->host.c_str(),
              req.path.c_str(), res.status);
    });

    if (write_len == -1) {
        esp_http_client_set_header(client, "Transfer-Encoding", "chunked");
    }

    return ESP_OK;
}

int esp_http_client_write(esp_http_client_handle_t client, const char *buffer, int len) {
    // accumulate data in body
    client->writeBody += std::string(buffer, len);
    return len;
}

int esp_http_client_fetch_headers(esp_http_client_handle_t client) {
    client->makeRequestIfNecessary();
    return client->contentLength;
}

// bool esp_http_client_is_chunked_response(esp_http_client_handle_t client) {}

int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len) {
    client->makeRequestIfNecessary();
    return client->stream->readsome(buffer, len);
}

int esp_http_client_get_status_code(esp_http_client_handle_t client) {
    client->makeRequestIfNecessary();
    return client->status;
}

int esp_http_client_get_content_length(esp_http_client_handle_t client) {
    client->makeRequestIfNecessary();
    return client->contentLength;
}

esp_err_t esp_http_client_close(esp_http_client_handle_t client) { return ESP_OK; }

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client) {
    delete client;
    return ESP_OK;
}

// esp_http_client_transport_t esp_http_client_get_transport_type(esp_http_client_handle_t client)
// {}

// esp_err_t esp_http_client_set_redirection(esp_http_client_handle_t client) {}

// void esp_http_client_add_auth(esp_http_client_handle_t client) {}

// bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client) {}

// int esp_http_client_read_response(esp_http_client_handle_t client, char *buffer, int len) {}

// esp_err_t esp_http_client_flush_response(esp_http_client_handle_t client, int *len) {}

// esp_err_t esp_http_client_get_url(esp_http_client_handle_t client, char *url, const int len)
// {}

// esp_err_t esp_http_client_get_chunk_length(esp_http_client_handle_t client, int *len) {}
