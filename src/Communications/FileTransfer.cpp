#include "FileTransfer.hpp"

#include <fstream>

#include "Models/Json.hpp"
#include "Models/Readable.hpp"

void FileTransfer::handleCommand(const std::string &command, const std::string &payload,
                                 const DirectCommand::TxLine &txLine) {
    if (awaitingFinalPutSuccess_) {
        if (command == "FILE" && payload == "PUT SUCCESS") {
            if (strcmp(fileType_, "EPUB") == 0) {
                txLine("TRANSFER BOOK SUCCESS");
            }
            resetState();
            return;
        }
    }
    if (awaitingFileGetSuccess_) {
        resetState();
        return;
    }
    if (command == "FILE" && HasPrefix(payload.c_str(), "PUT")) {
        handleFilePutCommand(command, payload, txLine);
    } else if (command == "FILE" && HasPrefix(payload.c_str(), "GET")) {
        handleFileGetCommand(command, payload, txLine);
    } else if (command == "FILE" && HasPrefix(payload.c_str(), "LS")) {
        handleFileLSCommand(command, payload, txLine);
    } else if (command == "TRANSFER" && HasPrefix(payload.c_str(), "BOOK")) {
        handleTransferBookCommand(command, payload, txLine);
    } else {
        txLine("ERROR");
    }
}

void FileTransfer::sendFileChunk() {
    size_t rxSize = 0;
    auto data = std::make_unique<uint8_t[]>(CHUNK_SIZE);
    while ((rxSize = fileGet_.read(data.get(), CHUNK_SIZE)) > 0) {
        usbSerial_.txSendData(data.get(), rxSize);
        totalGetTransferred_ += rxSize;
        if (totalGetTransferred_ % (CHUNK_SIZE * CHUNK_HANDSHAKE_INTERVAL) == 0) {
            awaitingCancelContinue_ = true;
            return;
        }
    }
    awaitingFileGetSuccess_ = true;
    fileGet_.close();
}

void FileTransfer::handleFileLSCommand(const std::string &command, const std::string &payload,
                                       const DirectCommand::TxLine &txLine) {
    char dirname[65]; // Large enough to hold the filename as a string
    if (sscanf(payload.c_str(), "LS %64s", dirname) == 1) {
        log_i("Directory name to list is: %s", dirname);
        std::string dirPath = StringFormat("%s", dirname);
        FileList fileListing;
        FileList dirListing;
        int retVal = fileSystem_.listDirWithSubdirectories(dirPath, fileListing, dirListing);
        if (retVal == -1) {
            log_e("Failed to open %s", dirPath.c_str());
            txLine("ERROR");
            return;
        }

        // Create a string that is ' ' seperated with all the elements of the fileListing vector
        std::string fileListingString;
        for (const auto &file : fileListing) {
            fileListingString += file + " ";
        }

        for (const auto &dir : dirListing) {
            fileListingString += dir + "/ ";
        }

        // get rid of the trailing space at the end (or / for dir)
        fileListingString.pop_back();

        txLine(command + " " + payload + " " + fileListingString);
    } else {
        txLine("ERROR");
    }
}

void FileTransfer::handleFileGetCommand(const std::string &command, const std::string &payload,
                                        const DirectCommand::TxLine &txLine) {
    char filename[65]; // Large enough to hold the filename as a string
    if (sscanf(payload.c_str(), "GET %64s", filename) == 1) {
        log_i("Filename to get is: %s", filename);
        std::string filePath = StringFormat("%s", filename);
        SFile file = fileSystem_.open(filePath, FILE_READ);
        if (!file || static_cast<int>(file.size()) == 0) {
            log_e("Failed to open %s", filePath.c_str());
            txLine("ERROR");
            file.close();
            return;
        }

        // Set up the ctx_ hash calculation context
        mbedtls_md_init(&ctx_);
        if (mbedtls_md_setup(&ctx_, mbedtls_md_info_from_type(mdType_), 0) != ESP_OK) {
            log_e("Failed to set up hash calculator");
            txLine("ERROR");
            file.close();
            return;
        }

        mbedtls_md_starts(&ctx_);

        // Iterate through the file and send it to the hash calculator in chunks
        size_t rxSize = 0;
        auto data = std::make_unique<uint8_t[]>(CHUNK_SIZE);
        while ((rxSize = file.read(data.get(), CHUNK_SIZE)) > 0) {
            mbedtls_md_update(&ctx_, data.get(), rxSize);
        }

        char hash[32];

        mbedtls_md_finish(&ctx_, reinterpret_cast<unsigned char *>(hash));

        std::string fileData = StringFormat("%d", file.size()) + " " +
                               BytesToHex(reinterpret_cast<const uint8_t *>(hash), 32);

        file.close();

        // Reopen the file for reading and transmitting
        // Make sure this works before sending the ACK
        fileGet_ = fileSystem_.open(filePath, FILE_READ);
        if (!fileGet_ || static_cast<int>(fileGet_.size()) == 0) {
            log_e("Failed to open for a second time %s", filePath.c_str());
            txLine("ERROR");
            return;
        }
        std::string commandToSend = command + " " + payload + " " + fileData;
        txLine(commandToSend);
        DispatchGetCore0BackgroundQueue()->dispatch([this] { sendFileChunk(); });
    } else {
        txLine("ERROR");
    }
}

void FileTransfer::handleFilePutCommand(const std::string &command, const std::string &payload,
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

    filePut_ = fileSystem_.open(filePath, FILE_WRITE, true);
    log_i("File opened for writing: %s", filePath.c_str());

    if (filePut_.getFilePtr() == nullptr) {
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

    directCommand_.setPassthroughData(
        [this](const std::shared_ptr<uint8_t[]> &data, size_t length) {
        fileTransferCallback(data, length);
    });

    txLine(StringFormat("%s %s", command.c_str(), payload.c_str()));
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
    size_t written = filePut_.write(data.get(), rxSize);
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
        if (bytesReceived_ > fileSize_) {
            log_w("Received too many bytes %d / %d", bytesReceived_, fileSize_);
        }

        filePut_.close();
        unsigned char hash[32];

        mbedtls_md_finish(&ctx_, hash);
        mbedtls_md_free(&ctx_);

        // Convert hash to hexadecimal string
        std::string hashHex = BytesToHex(reinterpret_cast<const uint8_t *>(hash), 32);

        if (memcmp(checksum_, hashHex.c_str(), 64) != 0) {
            log_e("Hash verification failed: expected %s and got %s (received %d/%d bytes)",
                  checksum_, hashHex.c_str(), bytesReceived_, fileSize_);
            unsetPassthroughAndSendPutError();
            return;
        }

        if (strcmp(fileType_, "JSON") == 0) {
            handleJSON();
        } else if (strcmp(fileType_, "EPUB") == 0) {
            handleEPUB();
        }

        directCommand_.unsetPassthroughData();
        usbSerial_.txSendLine("FILE PUT SUCCESS");
        awaitingFinalPutSuccess_ = true;
    }
}
