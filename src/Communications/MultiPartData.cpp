#include "MultiPartData.hpp"
#include <string.h>

void MultiPartData::addKeyValue(const std::string& key, const std::string& value) {
    addKeyValue(key, (uint8_t*)value.c_str(), value.length(), false);
}

void MultiPartData::addKeyValue(const std::string& key, uint8_t* value, int len, bool isFile) {
    char temp[1024];
    if (!keyWritten_) {
        tempFile_ = fileSystem_.open(fileName_, "rw", true);
        keyWritten_ = true;

        // write first boundary marker if this is the first key
        sprintf(temp, "--%s--", boundary_.c_str());
        tempFile_.write((uint8_t*)temp, strlen(temp));
    }

    // write form header
    sprintf(temp, "\r\nContent-Disposition: form-data; name=\"%s\"", key.c_str());
    tempFile_.write((uint8_t*)temp, strlen(temp));
    if (isFile) {
        sprintf(temp, "; filename=\"temp\"\r\n");
    } else {
        sprintf(temp, "\r\n\r\n");
    }
    
    tempFile_.write((uint8_t*)temp, strlen(temp));

    if (isFile) {
        sprintf(temp, "Content-Type: application/octet-stream\r\n\r\n");
        tempFile_.write((uint8_t*)temp, strlen(temp));
    }

    // now write the actual data
    tempFile_.write(value, len);
    sprintf(temp, "\r\n--%s", boundary_.c_str());
    tempFile_.write((uint8_t*)temp, strlen(temp));
}

int MultiPartData::sendPost() {
    
    char tmp[8];
    sprintf(tmp, "--\r\n");
    tempFile_.write((uint8_t*)tmp, strlen(tmp));

    char hdr[64];
    sprintf(hdr, "multipart/form-data; boundary=%s", boundary_.c_str());
    httpClient_->addHeader("Content-Type", hdr);
    httpClient_->setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpClient_->setTimeout(30000);

    // rewind the file
    tempFile_.flush();
    tempFile_.seek(0);

    int code = httpClient_->sendRequest("POST", &tempFile_, tempFile_.size());

    // close and delete the temp file
    tempFile_.close();
    fileSystem_.remove(fileName_);

    return code;
}