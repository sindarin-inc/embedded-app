#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "Communications/USBSerial.hpp"
#include "Misc/DirectCommand.hpp"
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
        directCommand_.addCommandHandler([this](const std::string &command) {
            if (command.rfind("FILE", 0) == 0 || command.rfind("TRANSFER", 0) == 0) {
                log_i("Got a FILE or TRANSFER command and handled in FileTransfer.hpp");
                handleCommand(command);
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
                directCommand_.setPassthroughData([this](const uint8_t *data, uint8_t length) {
                    fileTransferCallback(data, length);
                });
                awaitingCancelContinue_ = false;
                usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("CONTINUE"), 9);
                return true;
            }
            return false;
        });
    }
    void fileTransferCallback(const uint8_t *data, size_t rxSize);

private:
    void handleCommand(const std::string &command);
    void handleFileCommand(const std::string &command);
    void handleTransferBookCommand(const std::string &command);
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
        xTimerStop(timeoutTimer_, 0);
    }

    void timeoutTimerCallback(TimerHandle_t xTimer) {
        if (timerFed_ == false) {
            log_e("Timed out! Resetting state to accept other commands");
            unsetPassthroughAndSendPutError();
            xTimerStop(xTimer, 0);
        } else {
            timerFed_ = false;
        }
    }

    // Static trampoline function
    static void timerCallbackTrampoline(TimerHandle_t xTimer) {
        // Retrieve the instance pointer we stored as the timer ID
        auto *instance = static_cast<FileTransfer *>(pvTimerGetTimerID(xTimer));
        if (instance) {
            instance->timeoutTimerCallback(xTimer);
        }
    }

    FileSystem &fileSystem_;
    LocalStore &localStore_;
    USBSerial &usbSerial_;
    DirectCommand &directCommand_;

    char fileType_[10]{};
    char fileNameCharStr_[65]{};
    std::string fileName_;
    std::string fileUUID_;
    int fileSize_ = 0;
    int bytesReceived_ = 0;
    char checksum_[65]{};
    mbedtls_md_context_t ctx_{};
    mbedtls_md_type_t mdType_ = MBEDTLS_MD_SHA256;
    bool awaitingCancelContinue_ = false;
    bool awaitingFinalSuccess_ = false;
    TimerHandle_t timeoutTimer_ = nullptr;
    bool timerFed_ = false;
    SFile file_;
    std::unique_ptr<SpiRamJsonDocument> jsonDoc_ = nullptr;
};
