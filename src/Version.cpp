#include "Version.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <esp_app_desc.h>

char versionString[16] = "\0";

auto GitVersionString() -> const char * { return esp_app_get_description()->version; }

auto ExtractVersion(const std::string &input) -> std::string {
    std::size_t start = input.find("release/v") + 9; // 9 is length of "release/v"
    std::size_t end = input.find('-', start);

    // If no dash is found, use the end of the string
    if (end == std::string::npos) {
        end = input.length();
    }
    return input.substr(start, end - start);
}

auto SemanticVersionString() -> const char * {
    if (versionString[0] == '\0') {
        auto gitVersion = GitVersionString();
        ExtractVersion(gitVersion).copy(versionString, 15);
    }
    return versionString;
}
