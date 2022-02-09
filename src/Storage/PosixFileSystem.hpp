#include "Storage/FileSystem.hpp"

class PosixFileSystem : public FileSystem {
public:
    PosixFileSystem() noexcept : FileSystem(){};

    void begin() override;
    File open(const std::string &path) override;
    bool exists(const std::string &path) override;
};
