#include "esp_http_client.h"

#if CONFIG_USE_WIFI

#import <Foundation/Foundation.h>
#import <Foundation/NSURLSession.h>
#include <map>

#include "sindarin-debug.h"

class FakeClient {
public:
    FakeClient(const esp_http_client_config_t *config) : config(*config) {
        // Parse URL components
        NSString *urlString = [NSString stringWithUTF8String:config->url];
        NSURL *url = [NSURL URLWithString:urlString];

        host = [[url absoluteString] UTF8String];
        path = [[url path] UTF8String];

        // Use shared session instead of creating a new one
        session = [NSURLSession sharedSession];
        [(NSURLSession *)session retain];
    }

    ~FakeClient() {
        if (session) {
            [(NSURLSession *)session release];
        }
        if (responseData) {
            [(NSMutableData *)responseData release];
        }
    }

    esp_http_client_config_t config;
    void *session;      // NSURLSession*
    void *responseData; // NSMutableData*

    // Request state
    std::string host;
    std::string path;
    std::string contentType;
    std::string writeBody;
    std::map<std::string, std::string> headers;

    // Response state
    int status = 0;
    int contentLength = 0;
    std::map<std::string, std::string> responseHeaders;
    size_t readOffset = 0;

    void makeRequestIfNecessary() {
        if (requestMade_) {
            return;
        }
        requestMade_ = true;

        NSString *urlString = [NSString stringWithUTF8String:host.c_str()];
        NSURL *url = [NSURL URLWithString:urlString];

        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];

        // Set User-Agent
        [request setValue:@"SolSimIos" forHTTPHeaderField:@"User-Agent"];

        // Set method
        if (config.method == HTTP_METHOD_POST) {
            [request setHTTPMethod:@"POST"];
            if (!writeBody.empty()) {
                NSData *bodyData =
                    [NSData dataWithBytes:writeBody.c_str() length:writeBody.length()];
                [request setHTTPBody:bodyData];
            }
        }

        // Set headers
        for (const auto &header : headers) {
            NSString *key = [NSString stringWithUTF8String:header.first.c_str()];
            NSString *value = [NSString stringWithUTF8String:header.second.c_str()];
            [request setValue:value forHTTPHeaderField:key];
        }

        // Create semaphore for synchronous request
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

        responseData = [(NSMutableData *)[[NSMutableData alloc] init] retain];

        NSURLSessionDataTask *task = [(NSURLSession *)session
            dataTaskWithRequest:request
              completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                if (error) {
                    log_e("HTTP request error: %s", [[error localizedDescription] UTF8String]);
                    dispatch_semaphore_signal(semaphore);
                    return;
                }

                NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
                status = (int)[httpResponse statusCode];

                // Get response headers
                NSDictionary *respHeaders = [httpResponse allHeaderFields];
                for (NSString *key in respHeaders) {
                    NSString *value = [respHeaders objectForKey:key];
                    std::string keyStr = [key UTF8String];
                    std::string valueStr = [value UTF8String];
                    responseHeaders[keyStr] = valueStr;

                    // Call event handler for each header
                    esp_http_client_event_t event;
                    event.user_data = config.user_data;
                    event.event_id = HTTP_EVENT_ON_HEADER;
                    event.header_key = const_cast<char *>(keyStr.c_str());
                    event.header_value = const_cast<char *>(valueStr.c_str());
                    config.event_handler(&event);
                }

                // Store response data
                [(NSMutableData *)responseData appendData:data];
                contentLength = [data length];

                dispatch_semaphore_signal(semaphore);
              }];

        [task resume];

        // Wait for request to complete
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        dispatch_release(semaphore);
    }

protected:
    bool requestMade_ = false;
};

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config) {
    return new FakeClient(config);
}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key,
                                     const char *value) {
    if (std::string(key) == "Content-Type") {
        client->contentType = value;
    }
    client->headers[key] = value;
    return ESP_OK;
}

esp_err_t esp_http_client_get_header(esp_http_client_handle_t client, const char *key,
                                     char **value) {
    auto it = client->headers.find(key);
    if (it == client->headers.end()) {
        return ESP_ERR_NOT_FOUND;
    }
    *value = const_cast<char *>(it->second.c_str());
    return ESP_OK;
}

esp_err_t esp_http_client_open(esp_http_client_handle_t client, int write_len) {
    if (write_len == -1) {
        esp_http_client_set_header(client, "Transfer-Encoding", "chunked");
    }
    return ESP_OK;
}

int esp_http_client_write(esp_http_client_handle_t client, const char *buffer, int len) {
    client->writeBody += std::string(buffer, len);
    return len;
}

int esp_http_client_fetch_headers(esp_http_client_handle_t client) {
    client->makeRequestIfNecessary();
    return client->contentLength;
}

bool esp_http_client_is_chunked_response(esp_http_client_handle_t client) {
    auto it = client->responseHeaders.find("Transfer-Encoding");
    return it != client->responseHeaders.end() && it->second == "chunked";
}

int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len) {
    client->makeRequestIfNecessary();

    NSData *data = (NSData *)client->responseData;
    size_t remaining = [data length] - client->readOffset;
    if (remaining == 0) {
        return 0;
    }

    size_t bytesToRead = std::min((size_t)len, remaining);
    [data getBytes:buffer range:NSMakeRange(client->readOffset, bytesToRead)];
    client->readOffset += bytesToRead;

    return (int)bytesToRead;
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

int esp_http_client_read_response(esp_http_client_handle_t client, char *buffer, int len) {
    int read_len = 0;
    while (read_len < len) {
        int data_read = esp_http_client_read(client, buffer + read_len, len - read_len);
        if (data_read <= 0) {
            return read_len;
        }
        read_len += data_read;
    }
    return read_len;
}

#endif
