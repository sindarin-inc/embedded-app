#pragma once

#include <memory>
#include <string>
#include <vector>

#include "EPubDefs.hpp"

class EPubMetaFile {
public:
    EPubMetaFile(const char *path) : path_(path) {}

    void parse(const std::shared_ptr<uint8_t[]> &buffer, uint32_t length);

    std::vector<std::string> rootfilePaths;

private:
    const char *path_;
};
