#pragma once

#include <string>
#include <map>
#include "HTTPClient.h"
#include "Storage/FileSystem.hpp"

class MultiPartData {
public:
    MultiPartData(HTTPClient* client, FileSystem& fileSystem) 
    : httpClient_(client)
    , fileSystem_(fileSystem)
    , keyWritten_(false) {}
    ~MultiPartData() 
    {}

    void addKeyValue(const std::string& key, const std::string& value);
    void addKeyValue(const std::string& key, uint8_t* value, int len, bool isFile);
    int sendPost();
    
private:

    HTTPClient* httpClient_;
    FileSystem& fileSystem_;
    std::string boundary_ = "SIND12345";
    std::string fileName_ = "/temp.coredump";
    bool keyWritten_;
    File tempFile_;

};