#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "Communications/USBSerial.hpp"
#include "Misc/DirectCommand.hpp"
#include "Misc/InactivityTimer.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/LocalStore.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "mbedtls/md.h"
#include "sindarin-debug.h"

#define CHUNK_SIZE 1024
#define CHUNK_HANDSHAKE_INTERVAL 25

class FileTransfer {
public:
    FileTransfer(FileSystem &fileSystem, LocalStore &localStore, USBSerial &usbSerial,
                 DirectCommand &directCommand)
        : fileSystem_(fileSystem), localStore_(localStore), usbSerial_(usbSerial),
          directCommand_(directCommand) {
        directCommand_.addCommandHandler(
            [this](const std::string &command, const std::string &payload) {
            if (command == "FILE" || command == "TRANSFER") {
                log_i("Got a FILE or TRANSFER command and handled in FileTransfer.hpp");
                handleCommand(command, payload);
                return true;
            } else if (command == "CANCEL") {
                if (!awaitingCancelContinue_) {
                    log_e("Received a CANCEL command when not expecting one");
                }
                log_i("Transfer canceled by the sender");
                file_.close();
                usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("CANCEL"), 7);
                awaitingCancelContinue_ = false;
                return true;
            } else if (command == "CONTINUE") {
                if (!awaitingCancelContinue_) {
                    log_e("Received a CANCEL command when not expecting one");
                }
                log_i("Got a CONTINUE command");
                // turn back on the processing of file data
                directCommand_.setPassthroughData(
                    [this](const std::shared_ptr<uint8_t[]> &data, uint8_t length) {
                    fileTransferCallback(data, length);
                });
                awaitingCancelContinue_ = false;
                usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("CONTINUE"), 9);
                return true;
            }
            return false;
        });
    }
    void fileTransferCallback(const std::shared_ptr<uint8_t[]> &data, size_t rxSize);

private:
    void handleCommand(const std::string &command, const std::string &payload);
    void handleFileCommand(const std::string &command, const std::string &payload);
    void handleTransferBookCommand(const std::string &command, const std::string &payload);
    void handleEPUB();
    void handleJSON();
    void unsetPassthroughAndSendPutError();

    // Reset all the member variables to their initial state
    void resetState() {
        fileName_.clear();
        fileSize_ = 0;
        bytesReceived_ = 0;
        awaitingCancelContinue_ = false;
        awaitingFinalSuccess_ = false;
        timeoutTimer_.stop();
    }

    FileSystem &fileSystem_;
    LocalStore &localStore_;
    USBSerial &usbSerial_;
    DirectCommand &directCommand_;

    char fileType_[10]{};
    std::string fileName_;
    std::string fileUUID_;
    int fileSize_ = 0;
    int bytesReceived_ = 0;
    char checksum_[65]{};
    mbedtls_md_context_t ctx_{};
    mbedtls_md_type_t mdType_ = MBEDTLS_MD_SHA256;
    bool awaitingCancelContinue_ = false;
    bool awaitingFinalSuccess_ = false;
    InactivityTimer timeoutTimer_;
    SFile file_;
    std::unique_ptr<SpiRamJsonDocument> jsonDoc_ = nullptr;
};
