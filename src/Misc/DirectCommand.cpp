
#include "DirectCommand.hpp"

#include <esp_system.h>

#include "Communications/USBSerial.hpp"
#include "Dispatch.hpp"
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
    addCommandHandler([this](const std::string &command, const std::string &payload) {
        if (command == "PROGRAM") {
            USBSerial::getInstance().disableLogging();
            USBSerial::getInstance().initJtag();
            return true;
        } else if (command == "CONNECT") {
            log_d("Received CONNECT command");
            // This is just the connect handshake to indicate we're here and listening
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>("CONNECT"), 8);
            log_d("Sent CONNECT command");
            return true;
        } else if (command == "RESTART") {
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>("SUCCESS"), 8);
            // Wait 1 second before restarting so the receiver can get the success message
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
            return true;
        } else if (command == "DEVICE_INFO") {
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>("SUCCESS"), 8);

            // Build the response string of format
            // SERIAL_NUMBER,123456;MAC_ADDRESS,00:00:00:00:00:00;FREE_SPACE,123456
            std::string serialNumber = GetSerialNumber();
            std::string macAddress = GetMacAddress();
            std::string freeSpace = fileSystem_.getFreeSpace();

            std::string response = "SERIAL_NUMBER," + serialNumber + ";MAC_ADDRESS," + macAddress +
                                   ";FREE_SPACE," + freeSpace + "\n";

            log_d("Sending device info: %s with size %zu", response.c_str(), response.size());
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>(response.c_str()),
                                                response.size());
            return true;
        } else if (command == "MIRROR") {
            mirrorToSerial = !mirrorToSerial;
            USBSerial::getInstance().txSendData(reinterpret_cast<const uint8_t *>("SUCCESS"), 8);
            return true;
        }
        return false;
    });

    USBSerial::getInstance().registerListener(
        [this](const std::shared_ptr<uint8_t[]> &data, uint8_t length) {
        receiveData(data, length);
    });
}

void DirectCommand::handleCommand(const std::shared_ptr<uint8_t[]> &data, uint8_t length) {
    for (int i = 0; i < length; i++) {
        if (data[i] == '\n') {
            // found newline, process this command
            if (!commandHandlers_.empty() && !commandBuffer_.empty()) {
                auto commandBuffer = ResolveBackspace(commandBuffer_);
                DispatchGetCore0BackgroundQueue()->dispatch([this, commandBuffer] {
                    log_i("Command being received: %s with size %zu", commandBuffer.c_str(),
                          commandBuffer.size());
                    std::string commandId;
                    std::string payload;
                    auto spacePos = commandBuffer.find(' ');
                    if (spacePos != std::string::npos) {
                        commandId = commandBuffer.substr(0, spacePos);
                        payload = commandBuffer.substr(spacePos + 1);
                    } else {
                        commandId = commandBuffer;
                    }
                    // Uppercase the commandId
                    std::transform(commandId.begin(), commandId.end(), commandId.begin(),
                                   ::toupper);
                    for (auto &commandHandler : commandHandlers_) {
                        if (commandHandler(commandId, payload)) {
                            // if the command was handled, break out of this loop
                            break;
                        };
                    }
                });
            }
            commandBuffer_.clear();
        } else if (data[i] != '\r') {
            commandBuffer_ += data[i];
        }
    }
}

void DirectCommand::receiveData(const std::shared_ptr<uint8_t[]> &data, uint8_t length) {
    // Be sure we don't time out while receiving data
    powerManager_.feedInactivityTimer();
    if (passthroughDataFn_) {
        DispatchGetCore0BackgroundQueue()->dispatch(
            [this, data, length] { passthroughDataFn_(data, length); });
    } else {
        handleCommand(data, length);
    }
}
