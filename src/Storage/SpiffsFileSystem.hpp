#include "Storage/FileSystem.hpp"

class SpiffsFileSystem : public FileSystem {
public:
    SpiffsFileSystem() : FileSystem(){};

    void begin() override;
    auto open(const String &path) -> File override;
    auto exists(const String &path) -> bool override;
};
