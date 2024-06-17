
#include "DirectCommand.hpp"

#include "Communications/USBSerial.hpp"

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
    // The only comamnds this class implements are the PROGRAM command which
    // allows the device to enter programming mode and the RESTART command
    // which reboots the device.
    addCommandHandler([this](const std::string &command) {
        if (command == "PROGRAM") {
            USBSerial::getInstance().DisableLogging();
            USBSerial::getInstance().InitJtag();
        } else if (command == "RESTART") {
            esp_restart();
        }
    });

    USBSerial::getInstance().RegisterListener(
        [this](const uint8_t *data, size_t length) { receiveData(data, length); });
}

void DirectCommand::processCommand(const uint8_t *data, uint8_t length) {
    for (int i = 0; i < length; i++) {
        if (data[i] == '\n') {
            // found newline, process this command
            if (!commandHandlers_.empty() && !commandBuffer_.empty()) {
                commandBuffer_ = ResolveBackspace(commandBuffer_);
                for (auto &commandHandler_ : commandHandlers_) {
                    log_i("Command being sent: %s with size %d", commandBuffer_.c_str(),
                          commandBuffer_.size());
                    commandHandler_(commandBuffer_);
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
