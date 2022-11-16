#include "MultiPartData.hpp"
#include <string.h>

void MultiPartData::addKeyValue(const std::string& key, const std::string& value) {
    addKeyValue(key, (uint8_t*)value.c_str(), value.length(), false);
}

void MultiPartData::startKeyWrite() {
    char temp[128];
    if (!keyWritten_) {
        tempFile_ = fileSystem_.open(fileName_, FILE_WRITE, true);
        keyWritten_ = true;

        // write first boundary marker if this is the first key
        snprintf(temp, sizeof(temp), "--%s--", boundary_.c_str());
        tempFile_.write((uint8_t*)temp, strlen(temp));
    }
}

void MultiPartData::writeHeader(const std::string& key, bool isFile) {
    // write form header
    char temp[1024];
    snprintf(temp, sizeof(temp), "\r\nContent-Disposition: form-data; name=\"%s\"", key.c_str());
    tempFile_.write((uint8_t*)temp, strlen(temp));
    if (isFile) {
        // TODO: better filename
        snprintf(temp, sizeof(temp), "; filename=\"temp\"\r\n");
    } else {
        snprintf(temp, sizeof(temp), "\r\n\r\n");
    }
    
    tempFile_.write((uint8_t*)temp, strlen(temp));
}

void MultiPartData::writeFooter() {
    char temp[128];
    snprintf(temp, sizeof(temp), "\r\n--%s", boundary_.c_str());
    tempFile_.write((uint8_t*)temp, strlen(temp));
}

void MultiPartData::addKeyValue(const std::string& key, uint8_t* value, int len, bool isFile) {
    startKeyWrite();
    writeHeader(key, isFile);
    // now write the actual data
    tempFile_.write(value, len);
}

void MultiPartData::startFile(const std::string& key) {
    startKeyWrite();
    writeHeader(key, true);

    char temp[256];
    snprintf(temp, sizeof(temp), "Content-Type: application/octet-stream\r\n\r\n");
    tempFile_.write((uint8_t*)temp, strlen(temp));
}

void MultiPartData::writeFileChunk(uint8_t* chunk, int len) {
    tempFile_.write(chunk, len);
}

void MultiPartData::endFile() {
    writeFooter();
}

int MultiPartData::sendPost(HTTPClient* httpClient) {
    
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "--\r\n");
    tempFile_.write((uint8_t*)tmp, strlen(tmp));

    snprintf(tmp, sizeof(tmp), "multipart/form-data; boundary=%s", boundary_.c_str());
    httpClient->addHeader("Content-Type", tmp);
    httpClient->setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpClient->setTimeout(30000);

    tempFile_.close();
    tempFile_ = fileSystem_.open(fileName_, FILE_READ);
    int code = httpClient->sendRequest("POST", &tempFile_, tempFile_.size());

    return code;
}