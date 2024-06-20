#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "Communications/USBSerial.hpp"
#include "Misc/DirectCommand.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/LocalStore.hpp"
#include "sindarin-debug.h"

#define MAX_FILE_NAME_LENGTH 65
#define MAX_FILE_TITLE_LENGTH 129
#define MAX_FILE_AUTHOR_LENGTH 129

class FileTransfer {
public:
    FileTransfer(FileSystem &fileSystem, LocalStore &localStore, USBSerial &usbSerial,
                 DirectCommand &directCommand)
        : fileSystem_(fileSystem), localStore_(localStore), usbSerial_(usbSerial),
          directCommand_(directCommand) {
        directCommand_.addCommandHandler([this](const std::string &command) {
            if (command == "START_FILE_TRANSFER") {
                log_i("Got a START_FILE_TRANSFER and handled in FileTransfer.hpp");
                directCommand_.setPassthroughData([this](const uint8_t *data, uint8_t length) {
                    fileTransferCallback(data, length);
                });
            }
        });
    }
    void fileTransferCallback(const uint8_t *data, size_t length);

private:
    enum class FileReceiveState {
        FileName,
        FileTitle,
        FileAuthor,
        FileSize,
        FileChecksum,
        FileData
    };

    FileSystem &fileSystem_;
    LocalStore &localStore_;
    USBSerial &usbSerial_;
    DirectCommand &directCommand_;

    FileReceiveState fileReceiveState_ = FileReceiveState::FileName;
    char fileName_[MAX_FILE_NAME_LENGTH]{};
    char fileTitle_[MAX_FILE_TITLE_LENGTH]{};
    char fileAuthor_[MAX_FILE_AUTHOR_LENGTH]{};
    std::string fileUUID_;
    u_int fileSize_ = 0;
    u_int bytesReceived_ = 0;
    u_int checksum_ = 0;
    u_int runningChecksum_ = 0;
    SFile file_;
};