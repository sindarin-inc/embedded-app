#pragma once

#include <driver/sdmmc_host.h>
#include <sd_pwr_ctrl_by_on_chip_ldo.h>
#include <sdmmc_cmd.h>

#include "Storage/PosixFileSystem.hpp"

class SDFatFileSystem : public PosixFileSystem {
public:
    SDFatFileSystem() { setPrefix("/sdcard"); }
    SDFatFileSystem(SDFatFileSystem const &) = delete;
    ~SDFatFileSystem() override;

    auto begin() -> bool override;
    void close() override;
    auto format() -> bool override;

    auto getFreeSpace() -> std::string override;

protected:
    sdmmc_card_t *card_ = nullptr;
    sd_pwr_ctrl_handle_t pwrCtrlHandle_ = nullptr;
};
