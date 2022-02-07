#pragma once

#include <FS.h>
#include <vector>

#include "Contents/Readable.hpp"

// TODO: It's probably worth exploring `esp_vfs_t` as a generic filesystem interface
// Or we could checkout the Arduino FS class since it supports custom file systems via FSImpl
class FileSystem {
public:
    // Convenience methods
    void getReadables(std::vector<Readable> *readables);

    // Underlying methods to init and read data
    virtual void begin(){};
    // Note: File overrides bool and will be false if there's an error
    virtual auto open(const String &path) -> File { assert(0); };
    virtual auto exists(const String &path) -> bool { assert(0); };

protected:
    FileSystem() = default;
};
