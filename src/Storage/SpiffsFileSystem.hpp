#include "Storage/FileSystem.hpp"

class SpiffsFileSystem : public FileSystem {
public:
    SpiffsFileSystem() noexcept : FileSystem(){};

    auto begin() -> bool override;
    auto open(const std::string &path, const char *mode = FILE_READ,
              const bool create = false) const -> File override;
    [[nodiscard]] auto exists(const std::string &path) const -> bool override;
    auto listDir(const std::string &path, FileList &files) const -> int override;
    auto createDir(const std::string &path, bool allowExists = true) -> bool override;
    auto remove(const std::string &path) -> bool override;
};
