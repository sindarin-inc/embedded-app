#pragma once

#include "Storage/FileSystem.hpp"

class PosixFileSystem : public FileSystem {
public:
    PosixFileSystem() noexcept : FileSystem(){};
    PosixFileSystem(PosixFileSystem const &) = delete;

    auto begin() -> bool override;
    auto open(const std::string &path, const char *mode = FILE_READ,
              const bool create = false) const -> SFile override;
    [[nodiscard]] auto exists(const std::string &path) const -> bool override;
    auto listDir(const std::string &path, FileList &files, int limit = 0) const -> int override;
    auto listDirWithSubdirectories(const std::string &path, FileList &files, FileList &dirs) const
        -> int override;
    auto createDir(const std::string &path, bool allowExists = true) -> bool override;
    auto remove(const std::string &path) -> bool override;
    virtual void close();
    void setPrefix(const std::string &prefix);
    auto getPrefix() -> const char *;

    auto removeDir(const std::string &path) -> bool override;

protected:
    void createPath(char *output, size_t outputSize, const std::string &path) const;
};