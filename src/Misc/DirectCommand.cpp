
#include "DirectCommand.hpp"

#if !SIMULATOR
#include <driver/uart.h>
#endif
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
    addCommandHandler([this](const std::string &command, const std::string &payload,
                             const DirectCommand::TxLine &txLine) {
        // The following commands can run at any time even the earliest boot levels
        if (command == "ALLOWOFF") {
            AllowPowerOff();
            txLine(command);
            return true;
        } else if (command == "SLEEPOFF") {
            powerManager_.debugDisableLightSleep();
            txLine(command);
            return true;
        } else if (command == "SLEEPON") {
            powerManager_.debugEnableLightSleep();
            txLine(command);
            return true;
        } else if (command == "PROGRAM") {
            usbSerial_.disableLogging();
            usbSerial_.initJtag();
            return true;
        } else if (command == "RESTART") {
            txLine(command);
            powerManager_.reboot();
            return true;
        } else if (command == "DEVICE_INFO") {
            txLine(StringFormat("DEVICE_INFO SERIAL_NUMBER,%s;MAC_ADDRESS,%s;FREE_SPACE,%s",
                                GetSerialNumber().c_str(), GetMacAddress().c_str(),
                                fileSystem_.getFreeSpace().c_str()));
            return true;
        } else if (command == "VERSION") {
            txLine(StringFormat("VERSION SEMVER,%s;GIT,%s", SemanticVersionString(),
                                GitVersionString()));
            return true;
        } else if (command == "LOGGING_ON") {
            usbSerial_.enableLogging();
            usbSerial_.setLogLevel(ESP_LOG_DEBUG);
            txLine(command);
            return true;
        } else if (command == "LOGGING_OFF") {
            usbSerial_.disableLogging();
            txLine(command);
            return true;
        } else if (command == "LOGLEVEL") {
            if (payload.empty()) {
                txLine("LOGLEVEL ERROR");
                return true;
            }
            std::string level = payload;
            std::transform(level.begin(), level.end(), level.begin(), ::toupper);

            esp_log_level_t logLevel;
            if (level == "NONE") {
                logLevel = ESP_LOG_NONE;
            } else if (level == "ERROR") {
                logLevel = ESP_LOG_ERROR;
            } else if (level == "WARN") {
                logLevel = ESP_LOG_WARN;
            } else if (level == "INFO") {
                logLevel = ESP_LOG_INFO;
            } else if (level == "DEBUG") {
                logLevel = ESP_LOG_DEBUG;
            } else if (level == "VERBOSE") {
                logLevel = ESP_LOG_VERBOSE;
            } else {
                txLine("LOGLEVEL ERROR");
                return true;
            }
            usbSerial_.setLogLevel(logLevel);
            txLine(StringFormat("LOGLEVEL %s", level.c_str()));
            return true;
        } else if (command == "SERIAL") {
            txLine(StringFormat("SERIAL %s", GetSerialNumber().c_str()));
            return true;
        } else if (command == "USBPWR") {
            // Also log for convenience during testing
            auto response = StringFormat("USBPWR %d", USBConnected());
            log_i("%s", response.c_str());
            txLine(response);
            return true;
        }
        return false;
    });

    usbSerial_.registerListener([this](const std::shared_ptr<uint8_t[]> &data, size_t length) {
        receiveTusbPortData(data, length);
    });
}

void DirectCommand::handleCommand(const std::string &line, const DirectCommand::TxLine &txLine) {
    if (line.empty()) {
        return;
    }
    auto commandBuffer = ResolveBackspace(line);
    DispatchGetCore0BackgroundQueue()->dispatch([this, commandBuffer, txLine] {
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
        std::transform(commandId.begin(), commandId.end(), commandId.begin(), ::toupper);
        for (auto &commandHandler : commandHandlers_) {
            if (commandHandler(commandId, payload, txLine)) {
                // if the command was handled, break out of this loop
                break;
            };
        }
    });
}

void DirectCommand::receiveTusbPortData(const std::shared_ptr<uint8_t[]> &data, size_t length) {
    // Be sure we don't time out while receiving data
    powerManager_.feedInactivityTimer();
    if (passthroughDataFn_) {
        DispatchGetCore0BackgroundQueue()->dispatch(
            [this, data, length] { passthroughDataFn_(data, length); });
    } else {
        for (int i = 0; i < length; i++) {
            if (data[i] == '\n') {
                // found newline, process this command
                handleCommand(tusbCommandBuffer_,
                              [this](const std::string &str) { usbSerial_.txSendLine(str); });
                tusbCommandBuffer_.clear();
            } else if (data[i] != '\r') {
                tusbCommandBuffer_ += data[i];
            }
        }
    }
}
void DirectCommand::pollHwUart() {
    // Handle debug commands early so that we can avoid sleep if necessary
    // Spend the rest of the time waiting for debug commands (that will probably never come)
#if !SIMULATOR
    uint8_t data[64];
    auto length = uart_read_bytes(UART_NUM_0, data, sizeof(data), 0);
    for (int i = 0; i < length; i++) {
        if (data[i] == '\n') {
            // found newline, process this command
            handleCommand(hwUartCommandBuffer_, [](const std::string &str) {
                uart_write_bytes(UART_NUM_0, str.c_str(), str.length());
                uart_write_bytes(UART_NUM_0, "\n", 1);
            });
            hwUartCommandBuffer_.clear();
        } else if (data[i] != '\r') {
            hwUartCommandBuffer_ += data[i];
        }
    }
#endif
}
