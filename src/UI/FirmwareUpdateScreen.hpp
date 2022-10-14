#ifndef FIRMWARE_UPDATE_SCREEN_HPP
#define FIRMWARE_UPDATE_SCREEN_HPP

#include <string>
#include <utility>

#include "Communications/FirmwareDownload.hpp"
#include "Communications/WiFi.hpp"
#include "UI/Screen.hpp"

class FirmwareUpdateScreen : public Screen {
public:
    FirmwareUpdateScreen(DisplaySystem &display, const std::string &url, const std::string &md5,
                         std::string version)
        : display_(display), firmwareDownload_(url, md5), version_(std::move(version)){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    void drawMessage(std::string &message);

    DisplaySystem &display_;
    FirmwareDownload firmwareDownload_;
    std::string version_;
};

#endif