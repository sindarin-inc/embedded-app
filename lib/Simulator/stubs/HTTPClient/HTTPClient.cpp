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
 * connected
 * @return connected status
 */
bool HTTPClient::connected() {
    if (client_) {
        return client_->is_socket_open() != 0;
    }
    return false;
}

/**
 * end
 * called after the payload is handled
 */
void HTTPClient::end(void) {
    delete client_;
    delete stream_;
    // delete string_;
}

/**
 * send a GET request
 * @return http code
 */
int HTTPClient::GET() {
    log_d("Making GET request to %s%s", host_.c_str(), path_.c_str());
    client_->set_follow_location(true);
    client_->set_logger([](const httplib::Request &req, const httplib::Response &res) {
        log_d("GET %s: %d", req.path.c_str(), res.status);
        // log_d("Headers:");
        // for (auto it = req.headers.begin(); it != req.headers.end(); ++it) {
        //     log_d("  %s: %s", it->first.c_str(), it->second.c_str());
        // }
    });
    auto res = client_->Get(path_.c_str());
    if (res) {
        // Warning: for a certain size string this fails
        // string_ = new String(res->body.c_str());
        string_ = res->body;
        stream_ = new StringStream(res->body);
        log_d("Got response %d %d", res->body.size(), stream_->size());
        size_ = res->body.length();
    } else {
        log_e("Bad response object");
        return 0;
    }

    return res->status;
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
        // string_ = new String(res->body.c_str());
        string_ = res->body;
        stream_ = new StringStream(res->body);
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

StringStream *HTTPClient::getStreamPtr(void) { return stream_; }

/**
 * converts error code to String
 * @param error int
 * @return String
 */
String HTTPClient::errorToString(int error) {
    switch (error) {
    case HTTPC_ERROR_CONNECTION_REFUSED:
        return F("connection refused");
    case HTTPC_ERROR_SEND_HEADER_FAILED:
        return F("send header failed");
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
        return F("send payload failed");
    case HTTPC_ERROR_NOT_CONNECTED:
        return F("not connected");
    case HTTPC_ERROR_CONNECTION_LOST:
        return F("connection lost");
    case HTTPC_ERROR_NO_STREAM:
        return F("no stream");
    case HTTPC_ERROR_NO_HTTP_SERVER:
        return F("no HTTP server");
    case HTTPC_ERROR_TOO_LESS_RAM:
        return F("too less ram");
    case HTTPC_ERROR_ENCODING:
        return F("Transfer-Encoding not supported");
    case HTTPC_ERROR_STREAM_WRITE:
        return F("Stream write error");
    case HTTPC_ERROR_READ_TIMEOUT:
        return F("read Timeout");
    default:
        return String();
    }
}

/**
 * return all payload as String (may need lot of ram or trigger out of memory!)
 * @return String
 */
String HTTPClient::getString(void) { return String(string_.c_str()); }

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
