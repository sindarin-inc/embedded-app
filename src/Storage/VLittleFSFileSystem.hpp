#pragma once

#include "Storage/PosixFileSystem.hpp"

class VLittleFSFileSystem : public PosixFileSystem {
public:
    VLittleFSFileSystem() {
        setPrefix("/littlefs");
    }

    auto begin() -> bool override;
    void close() override;
protected:
    bool format();
};
