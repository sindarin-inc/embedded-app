
#include "DirectCommand.hpp"

#include <esp_system.h>

#include "Communications/USBSerial.hpp"
#include "Dispatch.hpp"
#include "Platform.hpp"
#include "Version.hpp"

extern DirectCommand directCommand;

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
    auto &usbSerial = USBSerial::getInstance();

    addCommandHandler([this, &usbSerial](const std::string &command, const std::string &payload) {
        // The following commands can run at any time even the earliest boot levels
        if (command == "ALLOWOFF") {
            AllowPowerOff();
            usbSerial.txSendLine(command);
            return true;
        } else if (command == "SLEEPOFF") {
            powerManager_.debugDisableLightSleep();
            usbSerial.txSendLine(command);
            return true;
        } else if (command == "SLEEPON") {
            powerManager_.debugEnableLightSleep();
            usbSerial.txSendLine(command);
            return true;
        } else if (command == "PROGRAM") {
            usbSerial.disableLogging();
            usbSerial.initJtag();
            return true;
        } else if (command == "RESTART") {
            usbSerial.txSendLine(command);
            // Wait 1 second before restarting so the receiver can get the success message
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
            return true;
        } else if (command == "DEVICE_INFO") {
            usbSerial.txSendLine(
                StringFormat("DEVICE_INFO SERIAL_NUMBER,%s;MAC_ADDRESS,%s;FREE_SPACE,%s",
                             GetSerialNumber().c_str(), GetMacAddress().c_str(),
                             fileSystem_.getFreeSpace().c_str()));
            return true;
        } else if (command == "VERSION") {
            usbSerial.txSendLine(StringFormat("VERSION SEMVER,%s;GIT,%s", SemanticVersionString(),
                                              GitVersionString()));
            return true;
        } else if (command == "LOGGING_ON") {
            usbSerial.enableLogging();
            usbSerial.txSendLine(command);
            return true;
        } else if (command == "LOGGING_OFF") {
            usbSerial.disableLogging();
            usbSerial.txSendLine(command);
            return true;
        } else if (command == "SERIAL") {
            usbSerial.txSendLine(StringFormat("SERIAL %s", GetSerialNumber().c_str()));
            return true;
        } else if (command == "USBPWR") {
            // Also log for convenience during testing
            auto response = StringFormat("USBPWR %d", USBConnected());
            log_i("%s", response.c_str());
            usbSerial.txSendLine(response);
            return true;
        }
        return false;
    });

    usbSerial.registerListener([this](const std::shared_ptr<uint8_t[]> &data, uint8_t length) {
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
                    log_i("Received command: %s", commandBuffer.c_str());
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
