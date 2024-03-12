#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

class DebugCommand {
public:
    // Constructor that takes a callback std::string
    DebugCommand(std::function<void(std::string)> commandHandler)
        : commandHandler_(std::move(std::move(commandHandler))) {}
    void addData(uint8_t *data, uint8_t length);

private:
    std::string commandBuffer_;
    std::function<void(std::string)> commandHandler_;
};