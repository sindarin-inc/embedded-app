#if SIMULATOR

#include "PosixFileSystem.hpp"

#include <memory>
#include <unistd.h>

#include "PosixFileImpl.hpp"
void PosixFileSystem::begin() {}

auto PosixFileSystem::open(const String &path) -> File {
    char prefixedPath[200];
    getcwd(prefixedPath, sizeof(prefixedPath));
    strncat(prefixedPath, "/data", sizeof(prefixedPath) - 1);
    strncat(prefixedPath, path.c_str(), sizeof(prefixedPath) - 1);
    return File(std::make_shared<PosixFileImpl>(prefixedPath));
}

bool PosixFileSystem::exists(const String &path) {
    char prefixedPath[200];
    getcwd(prefixedPath, sizeof(prefixedPath));
    strncat(prefixedPath, "/data", sizeof(prefixedPath) - 1);
    strncat(prefixedPath, path.c_str(), sizeof(prefixedPath) - 1);
    struct stat buffer;
    return (stat(prefixedPath, &buffer) == 0);
}

#endif
