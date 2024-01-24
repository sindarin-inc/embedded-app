#pragma once

#include <utility>

#include "Storage/FileSystem.hpp"

class PosixFileSystem : public FileSystem {
public:
    PosixFileSystem(std::string prefix = "./data", std::string prefixPreload = "./preload") noexcept
        : prefixUserData_(std::move(prefix)), prefixPreloadData_(std::move(prefixPreload)){};
    PosixFileSystem(PosixFileSystem const &) = delete;
    ~PosixFileSystem() override = default;

    auto open(const std::string &path, const char *mode = FILE_READ, const bool create = false,
              bool usePreload = false) const -> SFile override;
    // NOTE: This calls `stat` under the hood and takes 4-8ms
    [[nodiscard]] auto exists(const std::string &path, bool usePreload = false) const
        -> bool override;
    // NOTE: This calls `stat` under the hood and takes 4-8ms
    [[nodiscard]] auto size(const std::string &path, bool usePreload = false) const
        -> size_t override;
    auto listDir(const std::string &path, FileList &files, int limit = 0,
                 bool usePreload = false) const -> int override;
    auto listDirWithSubdirectories(const std::string &path, FileList &files, FileList &dirs,
                                   bool usePreload = false) const -> int override;
    auto createDir(const std::string &path, bool allowExists = true, bool usePreload = false)
        -> bool override;
    auto remove(const std::string &path, bool usePreload = false) -> bool override;
    auto removeDir(const std::string &path, bool usePreload) -> bool override;
    virtual void close();
    void setPrefix(std::string prefix) { prefixUserData_ = std::move(prefix); }
    auto getPrefix() -> std::string { return prefixUserData_; }
    auto setPrefixPreload(std::string prefix) { prefixPreloadData_ = std::move(prefix); }
    auto getPrefixPreload() -> std::string { return prefixPreloadData_; }

protected:
    std::string prefixUserData_;
    std::string prefixPreloadData_;
    [[nodiscard]] auto createPath(const std::string &path, bool usePreload) const -> std::string;
};
