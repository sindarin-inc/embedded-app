#include "VLittleFSFileSystem.hpp"

#include <dirent.h>
#if SIMULATOR
#include <stdio.h>
#include <sys/stat.h>
#else
#include <esp_littlefs.h>
#include <esp_vfs.h>
#endif
#include "sindarin-debug.h"

#if SIMULATOR
char VFileSystemPrefix[64] = "./data";
#else
char VFileSystemPrefix[64] = "/littlefs";
#endif

void sCreatePath(char *output, size_t outputSize, const std::string &path) {
    strncpy(output, VFileSystemPrefix, outputSize);
    strncat(output, path.c_str(), outputSize - 1);
}

auto VLittleFSFileSystem::begin() -> bool {
   // register littlefs filesystem
#if !SIMULATOR
    esp_vfs_littlefs_conf_t config = {
        .base_path = "/littlefs", .partition_label = "data", .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_littlefs_register(&config);
#endif
    return true;
}

auto VLittleFSFileSystem::open(const std::string &path, const char *mode, const bool create) const
    -> SFile {
    return sopen(path, mode, create);
}

auto VLittleFSFileSystem::sopen(const std::string &path, const char *mode, const bool create) const
    -> SFile {
    // Note(john): The version of framework-arduinoespressif32
    // (https://github.com/marcovannoord/arduino-esp32.git#idf-release/v4.0) has a slightly
    // different API here.
    char prefixedPath[200];
    sCreatePath(prefixedPath, sizeof(prefixedPath), path);
    auto file = SFile(std::make_shared<VFSFile>(prefixedPath));
    return file;
}

auto VLittleFSFileSystem::exists(const std::string &path) const -> bool {
    char prefixedPath[200];
    sCreatePath(prefixedPath, sizeof(prefixedPath), path);
    struct stat buffer;
    return (stat(prefixedPath, &buffer) == 0);
}

auto VLittleFSFileSystem::listDir(const std::string &path, FileList &files, int limit) const -> int {
    char prefixedPath[200];
    sCreatePath(prefixedPath, sizeof(prefixedPath), path);
    log_i("path: %s", prefixedPath);
    DIR *dp = opendir(prefixedPath);
    if (dp != NULL) {
        struct dirent *ep;
        int counted = 0;
        while ((ep = readdir(dp))) {
            if (ep->d_type != DT_DIR) {
                files.push_back(ep->d_name);
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
    return 0;
}

auto VLittleFSFileSystem::createDir(const std::string &path, bool allowExists) -> bool {
    char prefixedPath[200];
    sCreatePath(prefixedPath, sizeof(prefixedPath), path);
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

auto VLittleFSFileSystem::remove(const std::string &path) -> bool {
    char prefixedPath[200];
    sCreatePath(prefixedPath, sizeof(prefixedPath), path);

    return (::remove(prefixedPath));
}

void VLittleFSFileSystem::close() {
#if !SIMULATOR
    esp_vfs_littlefs_unregister("data");
#endif

}