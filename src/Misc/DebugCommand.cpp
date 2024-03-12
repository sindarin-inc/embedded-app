
#include "DebugCommand.hpp"

void DebugCommand::addData(uint8_t *data, uint8_t length) {
    for (int i = 0; i < length; i++) {
        if (data[i] == '\n') {
            // found newline, process this command
            if (commandHandler_ && !commandBuffer_.empty()) {
                commandHandler_(commandBuffer_);
            }
            commandBuffer_.clear();
        } else if (data[i] != '\r') {
            commandBuffer_ += toupper(data[i]);
        }
    }
}
