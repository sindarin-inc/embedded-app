#pragma once

#include <vector>

#include "Models/Readable.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/SFile.hpp"

typedef std::vector<std::string> FileList;

// TODO: It's probably worth exploring `esp_vfs_t` as a generic filesystem interface
// Or we could checkout the Arduino FS class since it supports custom file systems via FSImpl
// TODO: Alternately we could inherit from FS which would at least create a little less
// duplication potentially.
class FileSystem {
public:
    FileSystem(FileSystem const &) = delete;

    // Note: Allocates the ContainerStream on the heap, make sure to `delete` it somewhere.
    auto newRendererStream(const char *path) -> std::shared_ptr<RendererStream>;
    auto newGzipRendererStream(const char *path) -> std::shared_ptr<RendererStream>;

    // Underlying methods to init and read data
    virtual auto begin() -> bool { return true; };

    virtual auto open(const std::string &path, const char *mode = FILE_READ,
                      const bool create = false) const -> SFile {
        log_e("Abstract");
        assert(0);
        return {};
    };
    [[nodiscard]] virtual auto exists(const std::string &path) const -> bool {
        log_e("Abstract");
        assert(0);
        return false;
    };
    virtual auto listDir(const std::string &path, FileList &files, int limit = 0) const -> int {
        log_e("Abstract");
        assert(0);
        return 0;
    };
    virtual auto createDir(const std::string &path, bool allowExists = true) -> bool {
        log_e("Abstract");
        assert(0);
        return false;
    }
    virtual auto remove(const std::string &path) -> bool {
        log_e("Abstract");
        assert(0);
        return false;
    }
    virtual auto format() -> bool {
        log_e("Abstract");
        assert(0);
        return false;
    }

protected:
    FileSystem() noexcept = default;
};
