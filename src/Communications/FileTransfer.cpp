#include "FileTransfer.hpp"

#include <fstream>

#include "Models/Json.hpp"
#include "Models/Readable.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void print_stack_trace() {
    char *stack_ptr = (char *)__builtin_frame_address(0);
    ESP_LOGI("STACK", "Current stack pointer: %p", stack_ptr);

    // Print a few words from the stack
    for (int i = 0; i < 10; i++) {
        ESP_LOGI("STACK", "Stack[%d]: 0x%08lu", i, *((uint32_t *)(stack_ptr + i * 4)));
    }
}

void print_stack_size() {
    UBaseType_t stack_size = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("STACK", "Current task stack size (high water mark): %u bytes",
             stack_size * sizeof(StackType_t));
}

auto CalculateChecksum(const uint8_t *data, size_t length, uint8_t checksum = 0) -> uint8_t {
    uint8_t runningChecksum = checksum;
    for (size_t i = 0; i < length; ++i) {
        runningChecksum ^= data[i];
    }
    return runningChecksum;
}

void FileTransfer::handleCommand(const std::string &command) {
    if (awaitingFinalSuccess_) {
        if (command.find("FILE PUT SUCCESS") != std::string::npos) {
            // reset all state after successful file transfer
            awaitingFinalSuccess_ = false;
            awaitingCancelContinue_ = false;
            fileSize_ = 0;
            bytesReceived_ = 0;
            checksum_ = 0;
            runningChecksum_ = 0;
            return;
        }
    }
    if (command.find("FILE PUT") != std::string::npos) {
        handleFileCommand(command);
    } else if (command.find("TRANSFER BOOK") != std::string::npos) {
        handleTransferBookCommand(command);
    } else {
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
    }
}

void FileTransfer::handleFileCommand(const std::string &command) {
    char checksum_str[65]; // Large enough to hold the checksum as a string

    if (command.find("FILE PUT") != std::string::npos) {
        // Example: "FILE PUT EPUB 12345678 abcdef123456"
        sscanf(command.c_str(), "FILE PUT %s %d %s", fileType_, &fileSize_, checksum_str);

        log_i("File Type: %s", fileType_);
        log_i("File Size: %d", fileSize_);
        log_i("Checksum: %s", checksum_str);
        fileName_ = StringFormat("/book/%s.epub", fileUUID_.c_str());

        checksum_ = strtol(checksum_str, nullptr, 10); // Convert checksum to an integer
        log_i("Converted Checksum: %d", checksum_);

        fileReceiveState_ = FileReceiveState::FileData;
        runningChecksum_ = 0;
        bytesReceived_ = 0;

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

        // Prepare for the next FILE PUT command
        // directCommand_.setPassthroughData(
        //    [this](const uint8_t *data, uint8_t length) { fileTransferCallback(data, length); });
    } else {
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
    }
}

void FileTransfer::fileTransferCallback(const uint8_t *data, size_t rxSize) {
    size_t written = 0;
    bool success = true;
    bool sendSuccess = false;

    // print_stack_size();
    // print_stack_trace();

    switch (fileReceiveState_) {
    case FileReceiveState::FileData:
        written = file_.write(data, rxSize);
        runningChecksum_ = CalculateChecksum(data, rxSize, runningChecksum_);
        // log_d("Got some data of size %d and wrote %d", rxSize, written);
        if (written != rxSize) {
            log_e("Failed to write to file the right amount of bits");
            break;
        }
        // The below assumes that CHUNK_SIZE will be divisible by rxSize evenly. If not, all bets
        // are off! Figure out how to make this more robust.
        bytesReceived_ += rxSize;
        if ((bytesReceived_ < fileSize_) &&
            (bytesReceived_ % (CHUNK_SIZE * CHUNK_HANDSHAKE_INTERVAL) == 0)) {
            directCommand_.unsetPassthroughData();
            usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("CONTINUE"), 9);
            awaitingCancelContinue_ = true;
            return;
        }
        if (bytesReceived_ >= fileSize_) {

            UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
            log_d("end of single file receive high water mark: %d", watermark);
            // All done, reset the state for next time.
            file_.close();

            log_i("Closing file and checksum final value is %d and file size expected was %d and "
                  "got %d bytes",
                  runningChecksum_, fileSize_, bytesReceived_);

            if (checksum_ != runningChecksum_) {
                log_e("Checksum verification failed: expected %d and got %d", checksum_,
                      runningChecksum_);
                success = false;
            } else {
                if (strcmp(fileType_, "JSON") == 0) {
                    log_i("Processing JSON file");
                    std::string filePath = StringFormat("/book/%s.json", fileUUID_.c_str());
                    log_d("File path: %s", filePath.c_str());
                    SFile readablesFile = fileSystem_.open(filePath.c_str(), FILE_READ);
                    log_d("File opened for reading: %s", filePath.c_str());
                    if (!readablesFile || static_cast<int>(readablesFile.size()) == 0) {
                        log_e("Failed to open %s", filePath.c_str());
                        success = false;
                    }
                    auto docSize = static_cast<int>(readablesFile.size()) * 2;
                    log_d("Doc size: %d", docSize);
                    jsonDoc_ = std::make_unique<SpiRamJsonDocument>(docSize);
                    log_d("Json doc created");

                    auto error = deserializeJson(*jsonDoc_, readablesFile);
                    log_d("Deserialized JSON file");
                    readablesFile.close();
                    fileSystem_.remove(filePath.c_str());
                    if (error.code() != DeserializationError::Ok) {
                        log_e("Failed to deserialize readables file: %s", error.c_str());
                        success = false;
                    }
                } else if (strcmp(fileType_, "EPUB") == 0) {
                    log_i("Processing EPUB file %d bytes", bytesReceived_);
                    // Update the readable db on the glasses if the checksum checked out.
                    auto existingReadables = localStore_.books();
                    std::shared_ptr<Readable> newReadable = std::make_shared<Readable>();
                    newReadable->localOnly = true;
                    // Set the title
                    newReadable->title = (*jsonDoc_)["title"].as<std::string>();
                    // Set the UUID
                    newReadable->uuid = (*jsonDoc_)["uuid"].as<std::string>();
                    newReadable->readableType = (*jsonDoc_)["readable_type"].as<std::string>();

                    newReadable->authors.clear(); // Clear existing authors if any
                                                  // #if 0
                    JsonArray authorsArray = (*jsonDoc_)["authors"].as<JsonArray>();
                    for (JsonVariant authorVar : authorsArray) {
                        Author author;
                        author.uuid = authorVar["uuid"].as<std::string>();
                        author.name = authorVar["name"].as<std::string>();

                        // Handle the description, which might be null
                        if (!authorVar["description"].isNull()) {
                            author.description = authorVar["description"].as<std::string>();
                        } else {
                            author.description =
                                ""; // or however you want to handle null descriptions
                        }

                        newReadable->authors.push_back(author);
                    }

                    newReadable->dateCreated = (*jsonDoc_)["date_created"].as<std::string>();
                    newReadable->dateModified = (*jsonDoc_)["date_modified"].as<std::string>();
                    newReadable->datePublished = (*jsonDoc_)["date_published"].as<std::string>();
                    newReadable->processedBy = (*jsonDoc_)["processed_by"].as<std::string>();

                    //  Get the files array
                    JsonArray filesArray = (*jsonDoc_)["files"].as<JsonArray>();

                    // We'll use the first file in the array (index 0)
                    // If you need to handle multiple files, you'd loop through the array
                    if (filesArray.size() > 0) {
                        JsonObject fileObj = filesArray[0];

                        ReadableFile fileReadable;

                        // Fill in the fields
                        fileReadable.uuid = fileObj["uuid"].as<std::string>();
                        fileReadable.filetype = fileObj["filetype"].as<std::string>();
                        fileReadable.name = fileObj["name"].as<std::string>();
                        fileReadable.size =
                            fileObj["size"]
                                .as<size_t>(); // Assuming size_t is appropriate for file size
                        fileReadable.readableType = fileObj["readable_type"].as<std::string>();
                        newReadable->files.push_back(fileReadable);
                    }
                    localStore_.addReadable(existingReadables, newReadable);
                    localStore_.saveReadables(existingReadables);
                    jsonDoc_ = nullptr;
#if 0
#endif
                }
                directCommand_.unsetPassthroughData();
                sendSuccess = true;
            }

            // ResetState
            resetState();
        }
        break;
    case FileReceiveState::Idle:
        log_e("Sending ERROR from Idle state");
        // Should never get here so this is an error
        success = false;
        //        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
        break;
    default:
        log_e("Sending ERROR from default switch case");
        // Should never get here so this is an error
        success = false;
        // usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
        break;
    }

    if (success == true) {
        if (sendSuccess) {
            usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("FILE PUT SUCCESS"), 17);
            awaitingFinalSuccess_ = true;
        }
    } else {
        directCommand_.unsetPassthroughData();
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("FILE PUT ERROR"), 15);
    }
}
