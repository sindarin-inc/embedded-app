#include "Version.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <esp_app_desc.h>

char versionString[16] = "\0";

auto GitVersionString() -> const char * { return esp_app_get_description()->version; }

auto SemanticVersionString() -> const char * {
    if (versionString[0] == '\0') {
        auto gitVersion = GitVersionString();

        assert(strncmp(gitVersion, "release/v", 9) == 0 && "Bad GitVersion");

        const char *start = &(gitVersion[9]);
        const char *end = strchr(gitVersion, '-');
        if (end == nullptr) {
            return nullptr;
        }

        size_t length = end - start;
        strncpy(versionString, start, length);
    }
    return versionString;
}
