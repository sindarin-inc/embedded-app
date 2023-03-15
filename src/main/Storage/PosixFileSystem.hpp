#pragma once

#include "Storage/FileSystem.hpp"

class PosixFileSystem : public FileSystem {
public:
    PosixFileSystem() noexcept : FileSystem(){};

    auto begin() -> bool override;
    auto open(const std::string &path, const char *mode = FILE_READ,
              const bool create = false) const -> SFile;
    [[nodiscard]] auto exists(const std::string &path) const -> bool override;
    auto listDir(const std::string &path, FileList &files, int limit = 0) const -> int override;
    auto createDir(const std::string &path, bool allowExists = true) -> bool override;
    auto remove(const std::string &path) -> bool override;
    virtual void close();
    void setPrefix(const std::string &prefix);
    const char *getPrefix();

protected:
    void createPath(char *output, size_t outputSize, const std::string &path) const;
};