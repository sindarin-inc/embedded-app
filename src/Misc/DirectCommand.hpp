#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "Communications/USBSerial.hpp"
#include "Misc/PowerManager.hpp"
#include "Storage/FileSystem.hpp"
#include "sindarin-debug.h"

class DirectCommand {
public:
    static auto getInstance() -> DirectCommand &;
    // DirectCommand class
    // This class acts as the go-bewteen for the USB serial port and the rest of the system.
    // It listens for commands on the USB serial port and then calls any registered handler
    // that implements the command.
    // A command implementor (like FileCommand) can register a handler with DirectCommand
    // and then when a command is received, the handler will be called with the command string.
    //
    // Go into programming mode for flashing firmware
    // PROGRAM (enter programming mode)
    // RESTART (restart the device)
    // DEVICE_INFO (return basic device info like serial number, MAC address, and free space)
    //
    // Transfer a file over the USB serial connection
    // START_FILE_TRANSFER
    //

    DirectCommand(FileSystem &fileSystem, PowerManager &powerManager)
        : fileSystem_(fileSystem), powerManager_(powerManager){};
    void begin();
    void receiveData(const std::shared_ptr<uint8_t[]> &data, uint8_t length);
    void handleCommand(const std::shared_ptr<uint8_t[]> &data, uint8_t length);
    void addCommandHandler(
        std::function<bool(std::string commandId, std::string payload)> commandHandler) {
        commandHandlers_.push_back(std::move(commandHandler));
    }
    void setPassthroughData(
        std::function<void(std::shared_ptr<uint8_t[]> data, uint8_t length)> passthroughDataFn) {
        passthroughDataFn_ = std::move(passthroughDataFn);
    }

    void unsetPassthroughData() { passthroughDataFn_ = nullptr; };

private:
    FileSystem &fileSystem_;
    PowerManager &powerManager_;
    std::string commandBuffer_;
    std::vector<std::function<bool(std::string commandId, std::string payload)>> commandHandlers_;
    std::function<void(std::shared_ptr<uint8_t[]> data, uint8_t length)> passthroughDataFn_ =
        nullptr;
};