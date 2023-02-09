#include "LittleFSFileSystem.hpp"

#include <LittleFS.h>

#include "sindarin-debug.h"

auto LittleFSFileSystem::begin() -> bool {
    SHOW_HEAP();

    // Launch LITTLEFS file system
    // TODO: I've seen an issue but haven't debugged it where formatOnFail doesn't then begin the
    // filesystem afterwards so it takes a reboot before the FS can be used.

    if (!LittleFS.begin(true, "/littlefs", 255, "data")) {
        log_e("An Error has occurred while mounting LittleFS");
        return false;
    }

    SHOW_HEAP();
    return true;
}

auto LittleFSFileSystem::open(const std::string &path, const char *mode, const bool create) const
    -> File {
    // Note(john): The version of framework-arduinoespressif32
    // (https://github.com/marcovannoord/arduino-esp32.git#idf-release/v4.0) has a slightly
    // different API here.
    return LittleFS.open(path.c_str(), mode, create);
}

auto LittleFSFileSystem::exists(const std::string &path) const -> bool {
    return LittleFS.exists(path.c_str());
}

auto LittleFSFileSystem::listDir(const std::string &path, FileList &files, int limit) const -> int {
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
    return 0;
}

auto LittleFSFileSystem::createDir(const std::string &path, bool allowExists) -> bool {
    if (!LittleFS.mkdir(path.c_str())) {
        if (errno == EEXIST && allowExists) {
            log_d("Directory %s already exists", path.c_str());
        } else {
            log_e("Failed to create dir %s", path.c_str());
            return false;
        }
    }
    return true;
}

auto LittleFSFileSystem::remove(const std::string &path) -> bool {
    return LittleFS.remove(path.c_str());
}
