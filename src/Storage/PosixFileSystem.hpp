#pragma once

#include <utility>

#include "Storage/FileSystem.hpp"

class PosixFileSystem : public FileSystem {
public:
    PosixFileSystem(std::string prefix = "./data") noexcept
        : FileSystem(), prefix_(std::move(prefix)){};
    PosixFileSystem(PosixFileSystem const &) = delete;

    auto open(const std::string &path, const char *mode = FILE_READ,
              const bool create = false) const -> SFile override;
    [[nodiscard]] auto exists(const std::string &path) const -> bool override;
    auto listDir(const std::string &path, FileList &files, int limit = 0) const -> int override;
    auto listDirWithSubdirectories(const std::string &path, FileList &files, FileList &dirs) const
        -> int override;
    auto createDir(const std::string &path, bool allowExists = true) -> bool override;
    auto remove(const std::string &path) -> bool override;
    virtual void close();
    void setPrefix(std::string prefix) { prefix_ = std::move(prefix); }
    auto getPrefix() -> std::string { return prefix_; };

    auto removeDir(const std::string &path) -> bool override;

protected:
    std::string prefix_;
    [[nodiscard]] auto createPath(const std::string &path) const -> std::string;
};
