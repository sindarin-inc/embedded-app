#pragma once

#include <FS.h>
#include <vector>

#include "Contents/Readable.hpp"

typedef std::vector<std::string> FileList;

// TODO: It's probably worth exploring `esp_vfs_t` as a generic filesystem interface
// Or we could checkout the Arduino FS class since it supports custom file systems via FSImpl
class FileSystem {
public:
    // Convenience methods
    void getReadables(std::vector<Readable> *readables);

    // Underlying methods to init and read data
    virtual void begin(){};
    // Note: File overrides bool and will be false if there's an error
    virtual auto open(const std::string &path) const -> File {
        log_e("Abstract");
        assert(0);
        return File();
    };
    virtual auto exists(const std::string &path) const -> bool {
        log_e("Abstract");
        assert(0);
        return false;
    };
    virtual auto listDir(const std::string &path, FileList &files) const -> int {
        log_e("Abstract");
        assert(0);
        return 0;
    };
    virtual auto createDir(const std::string &path, bool allowExists = true) const -> bool {
        log_e("Abstract");
        assert(0);
        return false;
    }

protected:
    FileSystem() noexcept = default;
};
