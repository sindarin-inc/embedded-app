#include "HTTPClient.h"

#include <Arduino.h>
#include <base64.h>
#include <esp32-hal-log.h>

HTTPClient::HTTPClient() {}

HTTPClient::~HTTPClient() {}

/**
 * parsing the url for all needed parameters
 * @param url String
 */
bool HTTPClient::begin(String url) {
    // Because of the way httplib and HTTPClient are different
    // we need to parse the host out from the endpoint here.
    std::string urlStr = std::string(url.c_str());

    size_t found = urlStr.find_first_of(":");
    std::string protocol = urlStr.substr(0, found);

    std::string url_new = urlStr.substr(found + 3); // url_new is the url excluding the http part
    size_t found1 = url_new.find_first_of(":");
    size_t found2 = url_new.find_first_of("/");
    host_ = urlStr.substr(0, found2 + protocol.length() + 3);
    path_ = url_new.substr(found2);

    client_ = new httplib::Client(host_);

    return client_->is_valid();
}

/**
 * end
 * called after the payload is handled
 */
void HTTPClient::end(void) {
    delete client_;
    delete stream_;
    delete string_;
}

/**
 * sends a post request to the server
 * @param payload uint8_t *
 * @param size size_t
 * @return http code
 */
int HTTPClient::POST(uint8_t *payload, size_t size) {
    log_d("Making POST request to %s%s", host_.c_str(), path_.c_str());
    client_->set_follow_location(true);
    client_->set_logger([](const httplib::Request &req, const httplib::Response &res) {
        log_d("POST %s: %d", req.path.c_str(), res.status);
        // log_d("Headers:");
        // for (auto it = req.headers.begin(); it != req.headers.end(); ++it) {
        //     log_d("  %s: %s", it->first.c_str(), it->second.c_str());
        // }
    });
    auto res = client_->Post(path_.c_str(), (const char *)payload, size,
                             "application/x-www-form-urlencoded");
    if (res) {
        string_ = new String(res->body.c_str());
        stream_ = new StringStream(*string_);
        size_ = res->body.length();
    } else {
        log_e("Bad response object");
        return 0;
    }

    return res->status;
}

/**
 * size of message body / payload
 * @return -1 if no info or > 0 when Content-Length is set by server
 */
int HTTPClient::getSize(void) { return size_; }

/**
 * returns the stream of the tcp connection
 * @return WiFiClient
 */
StringStream &HTTPClient::getStream(void) { return *stream_; }

/**
 * return all payload as String (may need lot of ram or trigger out of memory!)
 * @return String
 */
String HTTPClient::getString(void) { return *string_; }

/**
 * adds Header to the request
 * @param name
 * @param value
 * @param first
 */
void HTTPClient::addHeader(const String &name, const String &value, bool first, bool replace) {
    // TODO: This may only allow setting a single header
    client_->set_default_headers({{name.c_str(), value.c_str()}});
}
