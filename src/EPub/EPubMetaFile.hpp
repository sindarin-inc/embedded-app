#pragma once

#include <string>
#include <vector>
#include <memory>

#include "EPubDefs.hpp"

class EPubMetaFile {
public:
    EPubMetaFile(const char *path) : path_(path) {}

    void parse(std::shared_ptr<uint8_t[]> buffer, uint32_t length);

    std::vector<std::string> rootfilePaths_;

private:
    const char *path_;
};
