#include "Storage/FileSystem.hpp"

class PosixFileSystem : public FileSystem {
public:
    PosixFileSystem() noexcept : FileSystem(){};

    void begin() override;
    auto open(const std::string &path) const -> File override;
    auto exists(const std::string &path) const -> bool override;
    auto listDir(const std::string &path, FileList &files) const -> int override;
    auto createDir(const std::string &path, bool allowExists = true) const -> bool override;
};
