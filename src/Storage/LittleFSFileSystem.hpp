#pragma once

#include "Storage/PosixFileSystem.hpp"

class LittleFSFileSystem : public PosixFileSystem {
public:
    LittleFSFileSystem() { setPrefix("/littlefs"); }
    LittleFSFileSystem(LittleFSFileSystem const &) = delete;

    auto begin() -> bool override;
    void close() override;
    auto format() -> bool override;
};
