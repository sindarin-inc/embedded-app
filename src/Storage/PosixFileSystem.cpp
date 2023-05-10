#include "PosixFileSystem.hpp"

#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "sindarin-debug.h"

char posixFileSystemPrefix[64] = "./data";

void PosixFileSystem::createPath(char *output, size_t outputSize, const std::string &path) const {
    strncpy(output, posixFileSystemPrefix, outputSize);
    strncat(output, path.c_str(), outputSize - 1);
}

auto PosixFileSystem::begin() -> bool { return true; }

auto PosixFileSystem::open(const std::string &path, const char *mode, const bool create) const
    -> SFile {
    char prefixedPath[200];
    createPath(prefixedPath, sizeof(prefixedPath), path);
    auto file = SFile(std::make_shared<VFSFile>(prefixedPath, mode, create));
    return file;
}

auto PosixFileSystem::exists(const std::string &path) const -> bool {
    char prefixedPath[200];
    createPath(prefixedPath, sizeof(prefixedPath), path);
    struct stat buffer;
    return (stat(prefixedPath, &buffer) == 0);
}

auto PosixFileSystem::listDir(const std::string &path, FileList &files, int limit) const -> int {
    char prefixedPath[200];
    createPath(prefixedPath, sizeof(prefixedPath), path);
    DIR *dp = opendir(prefixedPath);
    if (dp != nullptr) {
        struct dirent *ep;
        int counted = 0;
        while ((ep = readdir(dp))) {
            if (ep->d_type != DT_DIR) {
                files.emplace_back(ep->d_name);
                counted++;
            }
            if (limit > 0 && counted >= limit) {
                break;
            }
        }
        return closedir(dp);
    } else {
        perror("Couldn't open the directory");
        return -1;
    }
}

auto PosixFileSystem::listDirWithSubdirectories(const std::string &path, FileList &files,
                                                FileList &dirs) const -> int {
    char prefixedPath[200];
    createPath(prefixedPath, sizeof(prefixedPath), path);
    DIR *dp = opendir(prefixedPath);
    if (dp != nullptr) {
        struct dirent *ep;
        while ((ep = readdir(dp))) {
            if (ep->d_type != DT_DIR) {
                files.emplace_back(ep->d_name);
            } else if (strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0) {
                dirs.emplace_back(ep->d_name);
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
    createPath(prefixedPath, sizeof(prefixedPath), path);
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
    createPath(prefixedPath, sizeof(prefixedPath), path);

    return (::remove(prefixedPath) == 0);
}

void PosixFileSystem::close() {}

void PosixFileSystem::setPrefix(const std::string &prefix) {
    strncpy(posixFileSystemPrefix, prefix.c_str(), sizeof(posixFileSystemPrefix) - 1);
}

auto PosixFileSystem::getPrefix() -> const char * { return posixFileSystemPrefix; }

auto PosixFileSystem::removeDir(const std::string &path) -> bool {
    FileList files;
    FileList dirs;
    char fullPath[200];
    log_i("removeDir: %s", path.c_str());

    if (listDirWithSubdirectories(path, files, dirs) != 0) {
        log_e("failed to remove directory: %s", path.c_str());
        return false;
    }

    for (auto &file : files) {
        (void)snprintf(fullPath, sizeof(fullPath), "%s/%s", path.c_str(), file.c_str());
        log_i("deleting %s", file.c_str());
        bool removed = remove(fullPath);
        if (!removed) {
            log_e("failed to remove: %s", fullPath);
            return false;
        }
    }

    for (auto &dir : dirs) {
        log_i("deleting dir: %s", dir.c_str());
        // delete this directory
        (void)snprintf(fullPath, sizeof(fullPath), "%s/%s", path.c_str(), dir.c_str());
        if (!removeDir(fullPath)) {
            log_e("failed to remove: %s", fullPath);
            return false;
        }
    }

    // now remove this directory
    if (path.size() == 0) {
        return true;
    }

    createPath(fullPath, sizeof(fullPath), path);
    return (rmdir(fullPath) == 0);
}