#pragma once

#include <string>
#include <vector>

#include "Misc/ZipFile.hpp"

class EPubMetaFile {
public:
    EPubMetaFile(ZipFile &zipFile, const char *path) : path_(path), zipFile_(zipFile) {}

    void parse();

    std::vector<std::string> rootfilePaths;

private:
    const char *path_;
    ZipFile &zipFile_;
};
