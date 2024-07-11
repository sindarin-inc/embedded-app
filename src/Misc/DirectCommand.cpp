
#include "DirectCommand.hpp"

#include <esp_system.h>

#include "Communications/USBSerial.hpp"
#include "Platform.hpp"

extern DirectCommand directCommand;

bool mirrorToSerial = false;

auto DirectCommand::getInstance() -> DirectCommand & { return directCommand; }

// Remove backspace characters from the input string
auto ResolveBackspace(const std::string &input) -> std::string {
    std::string result;
    for (char c : input) {
        if (c == '\b') {
            if (!result.empty()) {
                result.pop_back(); // Remove the last character
            }
        } else {
            result.push_back(c); // Add the character to the result
        }
    }
    return result;
}

// Make sure this is called after the USBSerial instance is created
void DirectCommand::begin() {
    // The only comamnds this class implements are the PROGRAM command which
    // allows the device to enter programming mode and the RESTART command
    // which reboots the device. Also the DEVICE_INFO command.
    addCommandHandler([this](const std::string &command) {
        if (command == "PROGRAM") {
            USBSerial::getInstance().disableLogging();
            USBSerial::getInstance().initJtag();
        } else if (command == "RESTART") {
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>("SUCCESS"), 8);
            // Wait 1 second before restarting so the receiver can get the success message
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else if (command == "DEVICE_INFO") {
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>("SUCCESS"), 8);
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>("SERIAL_NUMBER,"),
                                                14);
            USBSerial::getInstance().txSendData(
                reinterpret_cast<const uint8_t *>(GetSerialNumber().c_str()),
                strlen(GetSerialNumber().c_str()));
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>(";"), 1);
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>("MAC_ADDRESS,"),
                                                12);
            USBSerial::getInstance().txSendData(
                reinterpret_cast<const uint8_t *>(GetMacAddress().c_str()),
                strlen(GetMacAddress().c_str()));
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>(";"), 1);
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>("FREE_SPACE,"),
                                                11);
            USBSerial::getInstance().txSendData(
                reinterpret_cast<const uint8_t *>(fileSystem_.getFreeSpace().c_str()),
                strlen(fileSystem_.getFreeSpace().c_str()));
        } else if (command == "MIRROR") {
            mirrorToSerial = !mirrorToSerial;
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>("SUCCESS"), 8);
        }
    });

    USBSerial::getInstance().registerListener(
        [this](const uint8_t *data, size_t length) { receiveData(data, length); });
}

void DirectCommand::processCommand(const uint8_t *data, uint8_t length) {
    for (int i = 0; i < length; i++) {
        if (data[i] == '\n') {
            // found newline, process this command
            if (!commandHandlers_.empty() && !commandBuffer_.empty()) {
                commandBuffer_ = ResolveBackspace(commandBuffer_);
                for (auto &commandHandler : commandHandlers_) {
                    log_i("Command being sent: %s with size %zu", commandBuffer_.c_str(),
                          commandBuffer_.size());
                    commandHandler(commandBuffer_);
                }
            }
            commandBuffer_.clear();
        } else if (data[i] != '\r') {
            commandBuffer_ += toupper(data[i]);
        }
    }
}
void DirectCommand::receiveData(const uint8_t *data, uint8_t length) {
    if (passthroughDataFn_) {
        passthroughDataFn_(data, length);
    } else {
        processCommand(data, length);
    }
}
