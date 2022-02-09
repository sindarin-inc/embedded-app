#include "Storage/FileSystem.hpp"

class SpiffsFileSystem : public FileSystem {
public:
    SpiffsFileSystem() noexcept : FileSystem(){};

    void begin() override;
    auto open(const std::string &path) -> File override;
    auto exists(const std::string &path) -> bool override;
};
