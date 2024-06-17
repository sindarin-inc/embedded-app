#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "Communications/USBSerial.hpp"
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
    //
    // Commands to simulate the remote control
    // REMOTE_UP
    // REMOTE_DOWN
    // REMOTE_ENTER
    // REMOTE_RIGHT
    // REMOTE_LEFT
    // REMOTE_BACK -- note this is the same as LEFT or escape in the Simulator
    //
    // Go into programming mode for flashing firmware
    // PROGRAM
    //
    // Transfer a file over the USB serial connection
    // START_FILE_TRANSFER
    // END_FILE_TRANSFER
    //
    // Put the grid on the page
    // GRID
    //
    // Get information about the available storage
    // REQUEST_STORAGE_INFO
    //
    // Get information about the device (to be implemented later)
    // REQUEST_DEVICE_INFO
    //
    // Constructor that takes a callback std::string
    DirectCommand() {}
    void begin();
    void receiveData(const uint8_t *data, uint8_t length);
    void processCommand(const uint8_t *data, uint8_t length);
    void addCommandHandler(std::function<void(std::string)> commandHandler) {
        commandHandlers_.push_back(std::move(commandHandler));
    }
    void setPassthroughData(std::function<void(const uint8_t *, uint8_t)> passthroughDataFn) {
        passthroughDataFn_ = passthroughDataFn;
    }

    void unsetPassthroughData() { passthroughDataFn_ = nullptr; }

private:
    std::string commandBuffer_;
    std::vector<std::function<void(std::string)>> commandHandlers_;
    std::function<void(const uint8_t *, uint8_t)> passthroughDataFn_ = nullptr;
};