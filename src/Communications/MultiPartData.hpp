#pragma once

#include <string>
#include <map>
#include "HTTPClient.h"
#include "Storage/FileSystem.hpp"

class MultiPartData {
public:
    MultiPartData(FileSystem& fileSystem) 
    : fileSystem_(fileSystem)
    , keyWritten_(false) {}
    ~MultiPartData() 
    {
        // close and delete the temp file
        tempFile_.close();
        fileSystem_.remove(fileName_);
    }

    void addKeyValue(const std::string& key, const std::string& value);
    void addKeyValue(const std::string& key, uint8_t* value, int len, bool isFile);
    void startFile(const std::string& key);
    void writeFileChunk(uint8_t* chunk, int len);
    void endFile();
    int sendPost(HTTPClient* httpClient);
    
private:

    FileSystem& fileSystem_;
    std::string boundary_ = "SIND12345";
    std::string fileName_ = "/temp.coredump";
    bool keyWritten_;
    File tempFile_;

    void startKeyWrite();
    void writeHeader(const std::string& key, bool isFile);
    void writeFooter();

};