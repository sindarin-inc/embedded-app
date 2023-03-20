#pragma once

#include "Storage/PosixFileSystem.hpp"

class LittleFSFileSystem : public PosixFileSystem {
public:
    LittleFSFileSystem() { setPrefix("/littlefs"); }

    auto begin() -> bool override;
    void close() override;

protected:
    auto format() -> bool;
};
