#include "LittleFSFileSystem.hpp"

#include <LittleFS.h>

#include "sindarin-debug.h"

auto LittleFSFileSystem::begin() -> bool {
    SHOW_HEAP();

    // Launch LITTLEFS file system
    if (!LittleFS.begin(false, "/littlefs", 255, "data")) {
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
    return LittleFS.open(path.c_str(), mode);
}

auto LittleFSFileSystem::exists(const std::string &path) const -> bool {
    return LittleFS.exists(path.c_str());
}

auto LittleFSFileSystem::listDir(const std::string &path, FileList &files) const -> int {
    File root = LittleFS.open(path.c_str());
    File file = root.openNextFile();
    while (file) {
        files.push_back(file.name());
        file = root.openNextFile();
    }
    return 0;
}

auto LittleFSFileSystem::createDir(const std::string &path, bool allowExists) -> bool {
    // LittleFS does not really have directories
    return true;
}

auto LittleFSFileSystem::remove(const std::string &path) -> bool {
    return LittleFS.remove(path.c_str());
}
