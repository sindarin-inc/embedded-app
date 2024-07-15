#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "Communications/USBSerial.hpp"
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

    DirectCommand(FileSystem &fileSystem) : fileSystem_(fileSystem){};
    void begin();
    void receiveData(const uint8_t *data, uint8_t length);
    void processCommand(const uint8_t *data, uint8_t length);
    void addCommandHandler(std::function<void(std::string)> commandHandler) {
        commandHandlers_.push_back(std::move(commandHandler));
    }
    void setPassthroughData(std::function<void(const uint8_t *, uint8_t)> passthroughDataFn) {
        passthroughDataFn_ = std::move(passthroughDataFn);
    }

    void unsetPassthroughData() { passthroughDataFn_ = nullptr; }

private:
    FileSystem &fileSystem_;
    std::string commandBuffer_;
    std::vector<std::function<void(std::string)>> commandHandlers_;
    std::function<void(const uint8_t *, uint8_t)> passthroughDataFn_ = nullptr;
};