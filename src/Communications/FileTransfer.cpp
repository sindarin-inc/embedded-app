// FileTransfer.cpp

#include "FileTransfer.hpp"

#include "Models/Readable.hpp"

#if SIMULATOR
#include "Storage/PosixFileSystem.hpp"
extern PosixFileSystem fileSystem;
#else
#include "Storage/LittleFSFileSystem.hpp"
extern LittleFSFileSystem fileSystem;
#endif

extern LocalStore localStore;

uint8_t calculate_checksum(const uint8_t *data, size_t length, uint8_t checksum = 0) {
    uint8_t checksum_ = checksum;
    for (size_t i = 0; i < length; ++i) {
        checksum_ ^= data[i];
    }
    return checksum_;
}

// Used to shorten the title for the UUID
void prepTitleForUUID(char *str, size_t length) {
    char *readPtr = str;
    char *writePtr = str;
    size_t i = 0;
    while (*readPtr != '\0') {
        if (!isspace(*readPtr)) {
            *writePtr = tolower(*readPtr);
            writePtr++;
        }
        readPtr++;
        i++;
    }

    *writePtr = '\0'; // Null-terminate the modified string
    // Be sure to truncate if needed.
    if (i > length) {
        str[length] = '\0';
    }
}

void FileTransfer::FileTransferCallback(const uint8_t *data, size_t length) {
    size_t rx_size = length;
    size_t written = 0;
    switch (fileReceiveState_) {
    case FileReceiveState::FileName:
        if (rx_size < MAX_FILE_NAME_LENGTH) {
            strncpy(fileName_, (char *)data, rx_size);
            fileName_[rx_size] = '\0';
            log_i("File Name: %s", fileName_);
            fileReceiveState_ = FileReceiveState::FileTitle;
        } else {
            log_e("Filename too long");
            // What is the right error handling here?
            fileReceiveState_ = FileReceiveState::FileData;
        }
        break;
    case FileReceiveState::FileTitle:
        if (rx_size < MAX_FILE_TITLE_LENGTH) {

            strncpy(fileTitle_, (char *)data, rx_size);
            fileTitle_[rx_size] = '\0';
            log_i("File Title: %s", fileTitle_);
            fileReceiveState_ = FileReceiveState::FileAuthor;
        } else {
            log_e("File title too long");
            fileReceiveState_ = FileReceiveState::FileData;
        }

        break;
    case FileReceiveState::FileAuthor:
        if (rx_size < MAX_FILE_AUTHOR_LENGTH) {
            strncpy(fileAuthor_, (char *)data, rx_size);
            fileAuthor_[rx_size] = '\0';
            log_i("File Author: %s", fileAuthor_);
            fileReceiveState_ = FileReceiveState::FileSize;
        } else {
            log_e("File author too long");
            fileReceiveState_ = FileReceiveState::FileData;
        }
        break;
    case FileReceiveState::FileSize:
        fileSize_ = atoi((char *)data);
        log_i("File Size: %d bytes", fileSize_);
        fileReceiveState_ = FileReceiveState::FileChecksum;
        break;
    case FileReceiveState::FileChecksum:
        char fileTitleForUUID[MAX_FILE_TITLE_LENGTH];
        checksum_ = atoi((char *)data);
        log_i("Checksum: %d", checksum_);
        strcpy(fileTitleForUUID, fileTitle_);
        prepTitleForUUID(fileTitleForUUID, 10);
        fileUUID_ = StringFormat("local%d%s", checksum_, fileTitleForUUID);
        log_i("File UUID: %s", fileUUID_.c_str());
        fileSystem.createDir("/book", true);
        file_ = fileSystem.open(
            StringFormat("%s%s%s", "/book/", fileUUID_.c_str(), ".epub").c_str(), FILE_WRITE, true);
        log_i("File opened for writing: %s",
              StringFormat("%s%s%s", "/book/", fileUUID_.c_str(), ".epub").c_str());
        if (file_.getFilePtr() == nullptr) {
            log_e("Failed to open file for writing");
        }
        fileReceiveState_ = FileReceiveState::FileData;
        break;
    case FileReceiveState::FileData:
        written = file_.write(data, rx_size);
        runningChecksum_ = calculate_checksum(data, rx_size, runningChecksum_);
        if (written != rx_size) {
            log_e("Failed to write to file the right amount of bits");
        } else {
            bytesReceived_ += rx_size;
            // log_i("Wrote a chunk to file with size %d and confirmed write bytes %d", rx_size,
            // written);
            if (bytesReceived_ >= fileSize_) {
                file_.close();
                log_i("Closing file and checksum final value is %d", runningChecksum_);
                // check checksum
                if (checksum_ == runningChecksum_) {
                    auto existingReadables = localStore.books();
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
                    localStore.addReadable(existingReadables, newReadable);
                    localStore.saveReadables(existingReadables);
                    USBSerial::getInstance().TxSendData((const uint8_t *)"SUCCESS", 8);
                } else {
                    log_e("Checksum verification failed");
                    USBSerial::getInstance().TxSendData((const uint8_t *)"FAILURE", 8);
                }
                fileReceiveState_ = FileReceiveState::FileName;
                bytesReceived_ = 0;
                fileSize_ = 0;
                checksum_ = 0;
                DirectCommand::getInstance().unsetPassthroughData();
            }
        }
        break;
    }
}