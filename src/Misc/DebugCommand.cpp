
#include "DebugCommand.hpp"

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

void DebugCommand::addData(uint8_t *data, uint8_t length) {
    for (int i = 0; i < length; i++) {
        if (data[i] == '\n') {
            // found newline, process this command
            if (commandHandler_ && !commandBuffer_.empty()) {
                commandBuffer_ = ResolveBackspace(commandBuffer_);
                commandHandler_(commandBuffer_);
            }
            commandBuffer_.clear();
        } else if (data[i] != '\r') {
            commandBuffer_ += toupper(data[i]);
        }
    }
}
