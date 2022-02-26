#pragma once

#include <FS.h>
#include <vector>

#include "Contents/Readable.hpp"
#include "Renderers/RendererStream.hpp"

typedef std::vector<std::string> FileList;

// TODO: It's probably worth exploring `esp_vfs_t` as a generic filesystem interface
// Or we could checkout the Arduino FS class since it supports custom file systems via FSImpl
// TODO: Alternately we could inherit from FS which would at least create a little less
// duplication potentially.
class FileSystem {
public:
    // Convenience methods
    void getReadables(std::vector<Readable> *readables);
    void getDownloadedPocketReadables(std::vector<Readable> *readables);
    // Note: Allocates the ContainerStream on the heap, make sure to `delete` it somewhere.
    auto newRendererStream(const char *path) -> ContainerStream<File> *;

    // Underlying methods to init and read data
    virtual void begin(){};

    virtual auto open(const std::string &path, const char *mode = FILE_READ,
                      const bool create = false) const -> File {
        log_e("Abstract");
        assert(0);
    };
    [[nodiscard]] virtual auto exists(const std::string &path) const -> bool {
        log_e("Abstract");
        assert(0);
    };
    virtual auto listDir(const std::string &path, FileList &files) const -> int {
        log_e("Abstract");
        assert(0);
    };
    virtual auto createDir(const std::string &path, bool allowExists = true) -> bool {
        log_e("Abstract");
        assert(0);
    }
    virtual auto remove(const std::string &path) -> bool {
        log_e("Abstract");
        assert(0);
    }

protected:
    FileSystem() noexcept = default;
};
