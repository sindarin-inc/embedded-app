#include "Storage/FileSystem.hpp"

class PosixFileSystem : public FileSystem {
public:
    PosixFileSystem() noexcept : FileSystem(){};

    void begin() override;
    [[nodiscard]] auto open(const std::string &path) const -> File override;
    [[nodiscard]] auto exists(const std::string &path) const -> bool override;
};
