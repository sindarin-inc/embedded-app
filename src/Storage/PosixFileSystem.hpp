#include "Storage/FileSystem.hpp"

class PosixFileSystem : public FileSystem {
public:
    PosixFileSystem() : FileSystem(){};

    void begin() override;
    File open(const String &path) override;
    bool exists(const String &path) override;
};
