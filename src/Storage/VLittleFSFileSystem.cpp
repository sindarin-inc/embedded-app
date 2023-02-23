#include "VLittleFSFileSystem.hpp"

#if SIMULATOR
#include <stdio.h>
#include <sys/stat.h>
#else
#include <esp_littlefs.h>
#include <esp_vfs.h>
#endif
#include "sindarin-debug.h"

char VFileSystemPrefix[64] = "./data";

void sCreatePath(char *output, size_t outputSize, const std::string &path) {
    strncpy(output, VFileSystemPrefix, outputSize);
    strncat(output, path.c_str(), outputSize - 1);
}

auto VLittleFSFileSystem::begin() -> bool {
    /*
    SHOW_HEAP();

    // Launch LITTLEFS file system
    // TODO: I've seen an issue but haven't debugged it where formatOnFail doesn't then begin the
    // filesystem afterwards so it takes a reboot before the FS can be used.

    if (!LittleFS.begin(true, "/littlefs", 255, "data")) {
        log_e("An Error has occurred while mounting LittleFS");
        return false;
    }

    SHOW_HEAP();
    */

   // register littlefs filesystem
#if !SIMULATOR
    esp_vfs_littlefs_conf_t config = {
        .base_path = "/littlefs", .partition_label = "data", .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_littlefs_register(&config);
#endif
    return true;
}

auto VLittleFSFileSystem::sopen(const std::string &path, const char *mode, const bool create) const
    -> SFile {
    // Note(john): The version of framework-arduinoespressif32
    // (https://github.com/marcovannoord/arduino-esp32.git#idf-release/v4.0) has a slightly
    // different API here.
    //return LittleFS.open(path.c_str(), mode, create);
    //return new SFile(path.c_str());

    //std::shared_ptr<VFSFile> f = std::make_shared<VFSFile>(path.c_str());
    std::shared_ptr<SFileImpl> f(new VFSFile(path.c_str()));
    //SFile file(std::dynamic_pointer_cast<SFileImpl, VFSFile>(f));
    SFile file(f);
    return file;
}

auto VLittleFSFileSystem::exists(const std::string &path) const -> bool {
    //return LittleFS.exists(path.c_str());

    // TODO: check for existence
    char prefixedPath[200];
    sCreatePath(prefixedPath, sizeof(prefixedPath), path);
    struct stat buffer;
    return (stat(prefixedPath, &buffer) == 0);
}

auto VLittleFSFileSystem::listDir(const std::string &path, FileList &files, int limit) const -> int {
    /*
    int counted = 0;
    File root = LittleFS.open(path.c_str());
    File file = root.openNextFile();
    while (file) {
        files.push_back(file.name());
        file.close();
        file = root.openNextFile();
        counted++;
        if (limit > 0 && counted >= limit) {
            break;
        }
    }
    root.close();
    */
    return 0;
}

auto VLittleFSFileSystem::createDir(const std::string &path, bool allowExists) -> bool {
    /*
    if (!LittleFS.mkdir(path.c_str())) {
        if (errno == EEXIST && allowExists) {
            log_d("Directory %s already exists", path.c_str());
        } else {
            log_e("Failed to create dir %s", path.c_str());
            return false;
        }
    }
    */
    return true;
}

auto VLittleFSFileSystem::remove(const std::string &path) -> bool {
    //return LittleFS.remove(path.c_str());
    return false;
}

void VLittleFSFileSystem::close() {
#if !SIMULATOR
    esp_vfs_littlefs_unregister("data");
#endif

}