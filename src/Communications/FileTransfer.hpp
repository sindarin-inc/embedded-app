// FileTransfer.hpp

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "Communications/USBSerial.hpp"
#include "Misc/DirectCommand.hpp"
#include "Storage/LocalStore.hpp"
#include "sindarin-debug.h"

class FileTransfer {
public:
    FileTransfer() {
        DirectCommand::getInstance().addCommandHandler([this](const std::string &command) {
            if (command == "START_FILE_TRANSFER") {
                log_i("Got a START_FILE_TRANSFER and handled in FileTransfer.hpp");
                DirectCommand::getInstance().setPassthroughData(
                    [this](const uint8_t *data, uint8_t length) {
                    FileTransferCallback(data, length);
                });
            }
        });
    }
    void FileTransferCallback(const uint8_t *data, size_t length);

private:
    int fileReceiveState_ = 1;
    char fileName_[65];
    u_int fileSize_ = 0;
    u_int bytesReceived_ = 0;
    u_int checksum_ = 0;
    SFile file_;
};