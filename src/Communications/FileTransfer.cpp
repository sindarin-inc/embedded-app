#include "FileTransfer.hpp"

#include <fstream>

#include "Models/Json.hpp"
#include "Models/Readable.hpp"

// the max amount of characters we want to turn the title into for use in a UUID
#define TITLE_UUID_LENGTH 10

auto CalculateChecksum(const uint8_t *data, size_t length, uint8_t checksum = 0) -> uint8_t {
    uint8_t runningChecksum = checksum;
    for (size_t i = 0; i < length; ++i) {
        runningChecksum ^= data[i];
    }
    return runningChecksum;
}

void FileTransfer::handleCommand(const std::string &command) {
    if (command.find("FILE SEND") != std::string::npos) {
        handleFileCommand(command);
    } else if (command.find("TRANSFER BOOK") != std::string::npos) {
        handleTransferBookCommand(command);
    } else {
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
    }
}

void FileTransfer::handleFileCommand(const std::string &command) {
    char checksum_str[65]; // Large enough to hold the checksum as a string

    if (command.find("FILE SEND") != std::string::npos) {
        // Example: "FILE SEND EPUB 12345678 abcdef123456"
        sscanf(command.c_str(), "FILE SEND %s %d %s", fileType_, &fileSize_, checksum_str);

        log_i("File Type: %s", fileType_);
        log_i("File Size: %d", fileSize_);
        log_i("Checksum: %s", checksum_str);
        fileName_ = StringFormat("/book/%s.epub", fileUUID_.c_str());

        checksum_ = strtol(checksum_str, nullptr, 16); // Convert checksum to an integer
        log_i("Converted Checksum: %d", checksum_);

        fileReceiveState_ = FileReceiveState::FileData;
        runningChecksum_ = 0;
        bytesReceived_ = 0;
        chunksReceived_ = 0;

        fileSystem_.createDir("/book", true);

        std::string filePath;
        if (strcmp(fileType_, "JSON") == 0) {
            filePath = StringFormat("/book/%s.json", fileUUID_.c_str());
        } else if (strcmp(fileType_, "EPUB") == 0) {
            filePath = StringFormat("/book/%s.epub", fileUUID_.c_str());
        } else {
            log_e("Unsupported file type: %s", fileType_);
            usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
            return;
        }

        file_ = fileSystem_.open(filePath.c_str(), FILE_WRITE, true);
        log_i("File opened for writing: %s", filePath.c_str());

        if (file_.getFilePtr() == nullptr) {
            log_e("Failed to open file for writing");
            usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
        } else {
            usbSerial_.txSendData(reinterpret_cast<const uint8_t *>(command.c_str()),
                                  command.length());
        }

        directCommand_.setPassthroughData(
            [this](const uint8_t *data, uint8_t length) { fileTransferCallback(data, length); });
    } else {
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
    }
}

void FileTransfer::handleTransferBookCommand(const std::string &command) {
    char book_uuid[65]; // Large enough to hold the book UUID as a string

    if (sscanf(command.c_str(), "TRANSFER BOOK %s", book_uuid) == 1) {
        log_i("Transfer Book UUID: %s", book_uuid);
        fileUUID_ = book_uuid;
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>(command.c_str()), command.length());

        // Prepare for the next FILE SEND command
        // directCommand_.setPassthroughData(
        //    [this](const uint8_t *data, uint8_t length) { fileTransferCallback(data, length); });
    } else {
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
    }
}

void FileTransfer::fileTransferCallback(const uint8_t *data, size_t rxSize) {
    size_t written = 0;
    switch (fileReceiveState_) {
    case FileReceiveState::FileData:
        written = file_.write(data, rxSize);
        runningChecksum_ = CalculateChecksum(data, rxSize, runningChecksum_);
        log_d("Got some data of size %d and wrote %d", rxSize, written);
        if (written != rxSize) {
            log_e("Failed to write to file the right amount of bits");
            break;
        }
        bytesReceived_ += rxSize;
        chunksReceived_ += 1;
        if ((bytesReceived_ < fileSize_) && (chunksReceived_ % CHUNK_HANDSHAKE_INTERVAL == 0)) {
            log_d("About to send CONTINUE");
            directCommand_.unsetPassthroughData();
            usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("CONTINUE"), 8);
            awaitingCancelContinue_ = true;
        }

        if (bytesReceived_ >= fileSize_) {
            // All done, reset the state for next time.
            file_.close();
            log_i("Closing file and checksum final value is %d", runningChecksum_);

            if (checksum_ != runningChecksum_) {
                log_e("Checksum verification failed");
                usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("FILE SEND ERROR"), 16);
            } else {
                if (strcmp(fileType_, "JSON") == 0) {
                    log_i("Processing JSON file");
                    // std::ifstream fileStream(StringFormat("/book/%s.json", fileUUID_.c_str()));
                    // json newReadable;
                    // fileStream >> newReadable;
                    // fileStream.close();
                    // Here you would merge the newReadable into readables.json
                    // For example, read readables.json, add newReadable and save it back.
                } else if (strcmp(fileType_, "EPUB") == 0) {
                    log_i("Processing EPUB file");
                    // Update the readable db on the glasses if the checksum checked out.
                    auto existingReadables = localStore_.books();
                    std::shared_ptr<Readable> newReadable = std::make_shared<Readable>();
                    newReadable->localOnly = true;
                    newReadable->title = fileName_;
                    newReadable->uuid = fileUUID_;
                    newReadable->authors = {{fileUUID_, fileName_, "No description"}};
                    newReadable->readableType = "book";
                    ReadableFile fileReadable;
                    fileReadable.uuid = fileUUID_;
                    fileReadable.filetype = "epub";
                    fileReadable.name = fileName_;
                    fileReadable.size = fileSize_;
                    fileReadable.readableType = "book";
                    newReadable->files.push_back(fileReadable);
                    localStore_.addReadable(existingReadables, newReadable);
                    localStore_.saveReadables(existingReadables);
                }
                usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("FILE SEND SUCCESS"), 18);
            }

            // ResetState
            fileReceiveState_ = FileReceiveState::Idle;
            bytesReceived_ = 0;
            fileSize_ = 0;
            directCommand_.unsetPassthroughData();
            checksum_ = 0;
            chunksReceived_ = 0;
        }
        break;
    case FileReceiveState::Idle:
        log_e("Sending ERROR from Idle state");
        // Should never get here so this is an error
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
        break;
    default:
        log_e("Sending ERROR from default switch case");
        // Should never get here so this is an error
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
        break;
    }
}
