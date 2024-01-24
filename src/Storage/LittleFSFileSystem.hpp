#pragma once

#include "Storage/PosixFileSystem.hpp"

class LittleFSFileSystem : public PosixFileSystem {
public:
    LittleFSFileSystem() {
        setPrefix("/littlefs");
        setPrefixPreload("/preload");
    }
    LittleFSFileSystem(LittleFSFileSystem const &) = delete;
    ~LittleFSFileSystem() override = default;

    auto begin() -> bool override;
    void close() override;
    auto format() -> bool override;

protected:
    auto attemptMount(std::string prefix, const char *partitionLabel) -> esp_err_t;
};
