#include "FileTransfer.hpp"

#include "Models/Readable.hpp"

// the max amount of characters we want to turn the title into for use
// in a UUID
#define TITLE_UUID_LENGTH 10

auto CalculateChecksum(const uint8_t *data, size_t length, uint8_t checksum = 0) -> uint8_t {
    uint8_t runningChecksum = checksum;
    for (size_t i = 0; i < length; ++i) {
        runningChecksum ^= data[i];
    }
    return runningChecksum;
}

// Used to shorten the title for the UUID
// This assumes the str passed in is longer than the maxOutputLength passed in
void PrepTitleForUuid(char *srcTitle, char *destUUIDTitle, size_t maxOutputLength) {
    char *readPtr = srcTitle;
    char *writePtr = destUUIDTitle;
    size_t i = 0;

    while (i < maxOutputLength && *readPtr != '\0') {
        if (!isspace(*readPtr)) {
            *writePtr = tolower(*readPtr);
            writePtr++;
            i++;
        }
        readPtr++;
    }

    *writePtr = '\0'; // Null-terminate the modified string
}

void FileTransfer::fileTransferCallback(const uint8_t *data, size_t length) {
    size_t rxSize = length;
    size_t written = 0;
    switch (fileReceiveState_) {
    case FileReceiveState::FileName:
        if (rxSize < MAX_FILE_NAME_LENGTH - 1) {
            strncpy(fileName_, reinterpret_cast<const char *>(data), rxSize);
            fileName_[rxSize] = '\0';
            log_i("File Name: %s", fileName_);
            fileReceiveState_ = FileReceiveState::FileTitle;
        } else {
            log_e("Filename too long");
            // What is the right error handling here?
            fileReceiveState_ = FileReceiveState::FileData;
        }
        break;
    case FileReceiveState::FileTitle:
        if (rxSize < MAX_FILE_TITLE_LENGTH - 1) {
            strncpy(fileTitle_, reinterpret_cast<const char *>(data), rxSize);
            fileTitle_[rxSize] = '\0';
            log_i("File Title: %s", fileTitle_);
            fileReceiveState_ = FileReceiveState::FileAuthor;
        } else {
            log_e("File title too long");
            fileReceiveState_ = FileReceiveState::FileData;
        }

        break;
    case FileReceiveState::FileAuthor:
        if (rxSize < MAX_FILE_AUTHOR_LENGTH - 1) {
            strncpy(fileAuthor_, reinterpret_cast<const char *>(data), rxSize);
            fileAuthor_[rxSize] = '\0';
            log_i("File Author: %s", fileAuthor_);
            fileReceiveState_ = FileReceiveState::FileSize;
        } else {
            log_e("File author too long");
            fileReceiveState_ = FileReceiveState::FileData;
        }
        break;
    case FileReceiveState::FileSize:
        fileSize_ = strtol(reinterpret_cast<const char *>(data), nullptr, 10);
        log_i("File Size: %d bytes", fileSize_);
        fileReceiveState_ = FileReceiveState::FileChecksum;
        break;
    case FileReceiveState::FileChecksum:
        char fileTitleForUUID[TITLE_UUID_LENGTH + 1];
        checksum_ = strtol(reinterpret_cast<const char *>(data), nullptr, 10);
        log_i("Checksum: %d", checksum_);
        PrepTitleForUuid(fileTitle_, fileTitleForUUID, TITLE_UUID_LENGTH);
        fileUUID_ = StringFormat("local%d%s", checksum_, fileTitleForUUID);
        log_i("File UUID: %s", fileUUID_.c_str());
        fileSystem_.createDir("/book", true);
        file_ =
            fileSystem_.open(StringFormat("/book/%s.epub", fileUUID_.c_str()), FILE_WRITE, true);
        log_i("File opened for writing: %s",
              StringFormat("/book/%s.epub", fileUUID_.c_str()).c_str());
        if (file_.getFilePtr() == nullptr) {
            log_e("Failed to open file for writing");
        }
        fileReceiveState_ = FileReceiveState::FileData;
        break;
    case FileReceiveState::FileData:
        written = file_.write(data, rxSize);
        runningChecksum_ = CalculateChecksum(data, rxSize, runningChecksum_);
        if (written != rxSize) {
            log_e("Failed to write to file the right amount of bits");
        } else {
            bytesReceived_ += rxSize;
            // log_i("Wrote a chunk to file with size %d and confirmed write bytes %d", rx_size,
            // written);
            if (bytesReceived_ >= fileSize_) {
                file_.close();
                log_i("Closing file and checksum final value is %d", runningChecksum_);
                // check checksum
                if (checksum_ == runningChecksum_) {
                    auto existingReadables = localStore_.books();
                    std::shared_ptr<Readable> newReadable = std::make_shared<Readable>();
                    newReadable->localOnly = true;
                    newReadable->title = fileTitle_;
                    newReadable->uuid = fileUUID_;
                    newReadable->authors = {{fileUUID_, fileAuthor_, "No description"}};
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
                    usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("SUCCESS"), 8);
                } else {
                    log_e("Checksum verification failed");
                    usbSerial_.txSendData(reinterpret_cast<const uint8_t *>("FAILURE"), 8);
                }
                fileReceiveState_ = FileReceiveState::FileName;
                bytesReceived_ = 0;
                fileSize_ = 0;
                checksum_ = 0;
                directCommand_.unsetPassthroughData();
            }
        }
        break;
    }
}