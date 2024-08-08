#include "FileTransfer.hpp"

#include <fstream>

#include "Models/Json.hpp"
#include "Models/Readable.hpp"

auto FileTransfer::calculateChecksum(const uint8_t *data, size_t length, uint8_t checksum)
    -> uint8_t {
    uint8_t runningChecksum = checksum;
    for (size_t i = 0; i < length; ++i) {
        runningChecksum ^= data[i];
    }
    return runningChecksum;
}

void FileTransfer::handleCommand(const std::string &command) {
    if (awaitingFinalSuccess_) {
        if (command.find("FILE PUT SUCCESS") != std::string::npos) {
            resetState();
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
    // Example: "FILE PUT EPUB 12345678 abcdef123456"
    sscanf(command.c_str(), "FILE PUT %s %d %s", fileType_, &fileSize_, checksum_str);
    fileName_ = StringFormat("/book/%s.epub", fileUUID_.c_str());
    checksum_ = strtol(checksum_str, nullptr, 10); // Convert checksum to an integer

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
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>(command.c_str()), command.length());
    }

    // Create a timer that triggers once after 5 seconds
    timeoutTimer_ = xTimerCreate("FileSendTimer",     // Timer name
                                 pdMS_TO_TICKS(5000), // Timer period in ticks (5000 ms = 5 seconds)
                                 pdTRUE,              // Auto-reload -- repeating timer
                                 static_cast<void *>(this), // Timer ID (passing 'this' pointer)
                                 timerCallbackTrampoline    // Static trampoline function
    );

    if (timeoutTimer_ == nullptr) {
        log_e("Failed to create timer");
    } else {
        // Start the timer
        if (xTimerStart(timeoutTimer_, 0) != pdPASS) {
            log_e("Failed to start timer");
        }
    }
    directCommand_.setPassthroughData(
        [this](const uint8_t *data, uint8_t length) { fileTransferCallback(data, length); });
}

void FileTransfer::handleTransferBookCommand(const std::string &command) {
    char book_uuid[65]; // Large enough to hold the book UUID as a string
    if (sscanf(command.c_str(), "TRANSFER BOOK %s", book_uuid) == 1) {
        log_i("Transfer Book UUID: %s", book_uuid);
        fileUUID_ = book_uuid;
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>(command.c_str()), command.length());
    } else {
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("ERROR"), 6);
    }
}

void FileTransfer::unsetPassthroughAndSendPutError() {
    directCommand_.unsetPassthroughData();
    usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("FILE PUT ERROR"), 15);
    resetState();
}

void FileTransfer::handleJSON() {
    std::string filePath = StringFormat("/book/%s.json", fileUUID_.c_str());
    SFile readablesFile = fileSystem_.open(filePath.c_str(), FILE_READ);
    if (!readablesFile || static_cast<int>(readablesFile.size()) == 0) {
        log_e("Failed to open %s", filePath.c_str());
        unsetPassthroughAndSendPutError();
        return;
    }
    auto docSize = static_cast<int>(readablesFile.size()) * 2;
    jsonDoc_ = std::make_unique<SpiRamJsonDocument>(docSize);
    auto error = deserializeJson(*jsonDoc_, readablesFile);
    readablesFile.close();
    fileSystem_.remove(filePath.c_str());
    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize readables file: %s", error.c_str());
        unsetPassthroughAndSendPutError();
    }
}

void FileTransfer::handleEPUB() {
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
            author.description = ""; // or however you want to handle null descriptions
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
            fileObj["size"].as<size_t>(); // Assuming size_t is appropriate for file size
        fileReadable.readableType = fileObj["readable_type"].as<std::string>();
        newReadable->files.push_back(fileReadable);
    }
    localStore_.addReadable(existingReadables, newReadable);
    localStore_.saveReadables(existingReadables);
    jsonDoc_ = nullptr;
}

void FileTransfer::fileTransferCallback(const uint8_t *data, size_t rxSize) {
    timerFed_ = true;
    size_t written = file_.write(data, rxSize);
    runningChecksum_ = calculateChecksum(data, rxSize, runningChecksum_);
    if (written != rxSize) {
        // Probably need to fail harder here somehow.
        log_e("Failed to write to file the right amount of bits");
        return;
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
        file_.close();
        if (checksum_ != runningChecksum_) {
            log_e("Checksum verification failed: expected %d and got %d", checksum_,
                  runningChecksum_);
            unsetPassthroughAndSendPutError();
            return;
        }
        if (strcmp(fileType_, "JSON") == 0) {
            handleJSON();
        } else if (strcmp(fileType_, "EPUB") == 0) {
            handleEPUB();
        }
        directCommand_.unsetPassthroughData();
        usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("FILE PUT SUCCESS"), 17);
        awaitingFinalSuccess_ = true;
    }
}
