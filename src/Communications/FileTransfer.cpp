// FileTransfer.cpp

#include "FileTransfer.hpp"

#include "Models/Readable.hpp"
#include "Storage/LocalStore.hpp"

#if SIMULATOR
#include "Storage/PosixFileSystem.hpp"
extern PosixFileSystem fileSystem;
#else
#include "Storage/LittleFSFileSystem.hpp"
extern LittleFSFileSystem fileSystem;
#endif

extern LocalStore localStore;

#define CONFIG_USB_CDC_RX_BUFSIZE 8192

uint8_t calculate_checksum_(const uint8_t *data, size_t length) {
    uint8_t checksum_ = 0;
    for (size_t i = 0; i < length; ++i) {
        checksum_ ^= data[i];
    }
    return checksum_;
}

void FileTransfer::FileTransferCallback(const uint8_t *data, size_t length) {
    size_t rx_size = length;
    if (fileReceiveState_ == 1) {
        strncpy(fileName_, (char *)data, rx_size);
        fileName_[rx_size] = '\0';
        log_i("File Name: %s", fileName_);
        fileReceiveState_ = 2;
    } else if (fileReceiveState_ == 2) {
        fileSize_ = atoi((char *)data);
        log_i("File Size: %d bytes", fileSize_);
        fileSystem.createDir("/book", true);
        file_ = fileSystem.open(StringFormat("%s%s", "/book/", "12345678.epub").c_str(), FILE_WRITE,
                                true);
        if (file_.getFilePtr() == nullptr) {
            log_e("Failed to open file for writing");
            fileReceiveState_ = 3;
        } else {
            checksum_ = 0;
            fileReceiveState_ = 3;
        }
    } else if (fileReceiveState_ == 3) {
        size_t written = file_.write(data, rx_size);
        bytesReceived_ += rx_size;
        // log_i("Wrote a chunk to file with size %d and confirmed write bytes %d", rx_size,
        // written);
        if (bytesReceived_ >= fileSize_) {
            file_.close();
            fileReceiveState_ = 4;
        }
    } else if (fileReceiveState_ == 4) {
        if (1) {
            auto existingReadables = localStore.books();
            std::shared_ptr<Readable> newReadable = std::make_shared<Readable>();
            newReadable->localOnly = true;
            newReadable->title = "The Hidden Life of Trees";
            newReadable->uuid = "12345678";
            newReadable->readableType = "book";
            ReadableFile fileReadable;
            fileReadable.uuid = "1234567890";
            fileReadable.filetype = "epub";
            fileReadable.name = fileName_;
            fileReadable.size = fileSize_;
            fileReadable.readableType = "book";
            newReadable->files.push_back(fileReadable);
            localStore.addReadable(existingReadables, newReadable);
            localStore.saveReadables(existingReadables);

            log_i("File received and saved successfully: %s", fileName_);
        } else {
            log_i("checksum_ verification failed");
        }
        fileReceiveState_ = 0;
        bytesReceived_ = 0;
        fileSize_ = 0;
        checksum_ = 0;
    }
}