#pragma once

#include <sdmmc_cmd.h>

#include "Storage/PosixFileSystem.hpp"

class SDFatFileSystem : public PosixFileSystem {
public:
    SDFatFileSystem() { setPrefix("/sdcard"); }
    SDFatFileSystem(SDFatFileSystem const &) = delete;
    ~SDFatFileSystem() override = default;

    auto begin() -> bool override;
    void close() override;
    auto format() -> bool override;

    auto getFreeSpace() -> std::string override;

protected:
    sdmmc_card_t *card = nullptr;
};
