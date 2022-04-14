#if SIMULATOR

#include "PosixFileSystem.hpp"

#include <dirent.h>
#include <memory>
#include <unistd.h>

#include "PosixFileImpl.hpp"

void createPath(char *output, size_t outputSize, const std::string &path) {
    strncpy(output, ".", outputSize);
    strncat(output, path.c_str(), outputSize - 1);
}

bool PosixFileSystem::begin() { return true; }

auto PosixFileSystem::open(const std::string &path, const char *mode, const bool create) const
    -> File {
    char prefixedPath[200];
    createPath(prefixedPath, sizeof(prefixedPath), std::string("/data") + path);
    auto file = File(std::make_shared<PosixFileImpl>(prefixedPath));
    return file;
}

bool PosixFileSystem::exists(const std::string &path) const {
    char prefixedPath[200];
    createPath(prefixedPath, sizeof(prefixedPath), std::string("/data") + path);
    struct stat buffer;
    return (stat(prefixedPath, &buffer) == 0);
}

auto PosixFileSystem::listDir(const std::string &path, FileList &files) const -> int {
    char prefixedPath[200];
    createPath(prefixedPath, sizeof(prefixedPath), std::string("/data") + path);
    DIR *dp = opendir(prefixedPath);
    if (dp != NULL) {
        struct dirent *ep;
        while ((ep = readdir(dp))) {
            if (ep->d_type != DT_DIR) {
                files.push_back(ep->d_name);
            }
        }
        return closedir(dp);
    } else {
        perror("Couldn't open the directory");
        return -1;
    }
}

auto PosixFileSystem::createDir(const std::string &path, bool allowExists) -> bool {
    char prefixedPath[200];
    createPath(prefixedPath, sizeof(prefixedPath), std::string("/data") + path);
    if (mkdir(prefixedPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
        if (errno == EEXIST && allowExists) {
            // Silently pass
        } else {
            log_e("Failed to create dir %s", path.c_str());
            return false;
        }
    }
    return true;
}

auto PosixFileSystem::remove(const std::string &path) -> bool {
    char prefixedPath[200];
    createPath(prefixedPath, sizeof(prefixedPath), std::string("/data") + path);

    return (::remove(prefixedPath));
};

#endif
