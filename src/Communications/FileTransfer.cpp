#include "FileTransfer.hpp"

#include <fstream>

#include "Models/Json.hpp"
#include "Models/Readable.hpp"

void FileTransfer::handleCommand(const std::string &command, const std::string &payload,
                                 const DirectCommand::TxLine &txLine) {
    if (awaitingFinalSuccess_) {
        if (command == "FILE" && payload == "PUT SUCCESS") {
            resetState();
            return;
        }
    }
    if (command == "FILE" && HasPrefix(payload.c_str(), "PUT")) {
        handleFileCommand(command, payload, txLine);
    } else if (command == "TRANSFER" && HasPrefix(payload.c_str(), "BOOK")) {
        handleTransferBookCommand(command, payload, txLine);
    } else {
        txLine("ERROR");
    }
}

void FileTransfer::handleFileCommand(const std::string &command, const std::string &payload,
                                     const DirectCommand::TxLine &txLine) {
    // Limited to
    char fileSizeStr[10];
    // Example: "FILE PUT EPUB 12345678 abcdef123456"
    int numParsed = sscanf(payload.c_str(), "PUT %9s %9s %64s", fileType_, fileSizeStr, checksum_);
    if (numParsed != 3) {
        txLine("ERROR");
        return;
    }

    fileSize_ = strtol(fileSizeStr, nullptr, 10);
    if (fileSize_ == 0) {
        txLine("ERROR");
        return;
    }

    fileName_ = StringFormat("/book/%s.epub", fileUUID_.c_str());

    bytesReceived_ = 0;

    fileSystem_.createDir("/book", true);

    std::string filePath;
    if (strcmp(fileType_, "JSON") == 0) {
        filePath = StringFormat("/book/%s.json", fileUUID_.c_str());
    } else if (strcmp(fileType_, "EPUB") == 0) {
        filePath = StringFormat("/book/%s.epub", fileUUID_.c_str());
    } else {
        log_e("Unsupported file type: %s", fileType_);
        txLine("ERROR");
        return;
    }

    file_ = fileSystem_.open(filePath, FILE_WRITE, true);
    log_i("File opened for writing: %s", filePath.c_str());

    if (file_.getFilePtr() == nullptr) {
        log_e("Failed to open file for writing");
        txLine("ERROR");
        return;
    }

    timeoutTimer_.start(
        [this]() {
        log_e("Timed out! Resetting state to accept other commands");
        unsetPassthroughAndSendPutError();
        },
        5000);

    // Set up the ctx_ hash calculation context

    mbedtls_md_init(&ctx_);
    if (mbedtls_md_setup(&ctx_, mbedtls_md_info_from_type(mdType_), 0) != ESP_OK) {
        log_e("Failed to set up hash calculator");
        txLine("ERROR");
        return;
    }

    mbedtls_md_starts(&ctx_);

    txLine(StringFormat("%s %s", command.c_str(), payload.c_str()));

    directCommand_.setPassthroughData(
        [this](const std::shared_ptr<uint8_t[]> &data, uint8_t length) {
        fileTransferCallback(data, length);
    });
}

void FileTransfer::handleTransferBookCommand(const std::string &command, const std::string &payload,
                                             const DirectCommand::TxLine &txLine) {
    char bookUuid[65]; // Large enough to hold the book UUID as a string
    if (sscanf(payload.c_str(), "BOOK %64s", bookUuid) == 1) {
        log_i("Transfer Book UUID: %s", bookUuid);
        fileUUID_ = bookUuid;
        txLine(StringFormat("%s %s", command.c_str(), payload.c_str()));
    } else {
        txLine("ERROR");
    }
}

void FileTransfer::unsetPassthroughAndSendPutError() {
    directCommand_.unsetPassthroughData();
    usbSerial_.txSendLine("FILE PUT ERROR");
    mbedtls_md_free(&ctx_);
    resetState();
}

void FileTransfer::handleJSON() {
    std::string filePath = StringFormat("/book/%s.json", fileUUID_.c_str());
    SFile readablesFile = fileSystem_.open(filePath, FILE_READ);
    if (!readablesFile || static_cast<int>(readablesFile.size()) == 0) {
        log_e("Failed to open %s", filePath.c_str());
        unsetPassthroughAndSendPutError();
        return;
    }
    auto docSize = static_cast<int>(readablesFile.size()) * 2;
    jsonDoc_ = std::make_unique<SpiRamJsonDocument>(docSize);
    auto error = deserializeJson(*jsonDoc_, readablesFile);
    readablesFile.close();
    fileSystem_.remove(filePath);
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

void FileTransfer::fileTransferCallback(const std::shared_ptr<uint8_t[]> &data, size_t rxSize) {
    timeoutTimer_.feed();
    size_t written = file_.write(data.get(), rxSize);
    // Update the hash calculation
    mbedtls_md_update(&ctx_, data.get(), rxSize);
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
        usbSerial_.txSendLine("CONTINUE");
        awaitingCancelContinue_ = true;
        return;
    }
    if (bytesReceived_ >= fileSize_) {
        file_.close();
        char hash[32];
        char hashHex[65];

        mbedtls_md_finish(&ctx_, reinterpret_cast<unsigned char *>(hash));

        // Convert hash to hexadecimal string
        for (int i = 0; i < 32; i++) {
            if (!snprintf(&hashHex[i * 2], 3, "%02x", static_cast<unsigned int>(hash[i]))) {
                log_e("Failed to convert hash to hex");
            };
        }

        if (memcmp(checksum_, hashHex, 64) != 0) {
            log_e("Hash verification failed: expected %s and got %s", checksum_, hashHex);
            unsetPassthroughAndSendPutError();
            return;
        }
        if (strcmp(fileType_, "JSON") == 0) {
            handleJSON();
        } else if (strcmp(fileType_, "EPUB") == 0) {
            handleEPUB();
        }

        mbedtls_md_free(&ctx_);
        directCommand_.unsetPassthroughData();
        usbSerial_.txSendLine("FILE PUT SUCCESS");
        awaitingFinalSuccess_ = true;
    }
}
