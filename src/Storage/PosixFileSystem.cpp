#include "PosixFileSystem.hpp"

#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "sindarin-debug.h"

auto PosixFileSystem::createPath(const std::string &path) const -> std::string {
    return StringFormat("%s%s", prefix_.c_str(), path.c_str());
}

auto PosixFileSystem::open(const std::string &path, const char *mode, const bool create) const
    -> SFile {
    auto prefixedPath = createPath(path);
    auto file = SFile(std::make_shared<VFSFile>(prefixedPath.c_str(), mode, create));
    return file;
}

auto PosixFileSystem::exists(const std::string &path) const -> bool {
    auto prefixedPath = createPath(path);
    struct stat buffer;
    return (stat(prefixedPath.c_str(), &buffer) == 0);
}

auto PosixFileSystem::size(const std::string &path) const -> size_t {
    auto prefixedPath = createPath(path);
    struct stat buffer;
    auto err = stat(prefixedPath.c_str(), &buffer);
    if (err != 0) {
        return 0;
    }
    return buffer.st_size;
}

auto PosixFileSystem::listDir(const std::string &path, FileList &files, int limit) const -> int {
    auto prefixedPath = createPath(path);
    DIR *dp = opendir(prefixedPath.c_str());
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
    auto prefixedPath = createPath(path);
    DIR *dp = opendir(prefixedPath.c_str());
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
    auto prefixedPath = createPath(path);
    if (mkdir(prefixedPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
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
    auto prefixedPath = createPath(path);
    return (::remove(prefixedPath.c_str()) == 0);
}

void PosixFileSystem::close() {}

auto PosixFileSystem::removeDir(const std::string &path) -> bool {
    FileList files;
    FileList dirs;
    log_i("removeDir: %s", path.c_str());

    if (listDirWithSubdirectories(path, files, dirs) != 0) {
        log_e("failed to remove directory: %s", path.c_str());
        return false;
    }

    for (auto &file : files) {
        auto fullPath = StringFormat("%s/%s", path.c_str(), file.c_str());
        log_i("deleting %s", file.c_str());
        bool removed = remove(fullPath.c_str());
        if (!removed) {
            log_e("failed to remove: %s", fullPath.c_str());
            return false;
        }
    }

    for (auto &dir : dirs) {
        log_i("deleting dir: %s", dir.c_str());
        // delete this directory
        auto fullPath = StringFormat("%s/%s", path.c_str(), dir.c_str());
        if (!removeDir(fullPath.c_str())) {
            log_e("failed to remove: %s", fullPath.c_str());
            return false;
        }
    }

    // now remove this directory
    if (path.size() == 0) {
        return true;
    }

    auto fullPath = createPath(path);
    return (rmdir(fullPath.c_str()) == 0);
}