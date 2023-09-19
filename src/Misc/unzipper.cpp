// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "unzipper.hpp"

#include <iomanip>
#include <iostream>

auto Unzipper::open() -> bool {

    // Open zip file
    if (!fileSystem_.exists(filePath_)) {
        log_e("File doesn´t exists: %s", filePath_.c_str());
        return false;
    }
    file_ = fileSystem_.open(filePath_, "r");
    if (!file_) {
        log_e("Unable to open file: %s", filePath_.c_str());
        return false;
    }
    isOpen_ = true;

    int err = 0;

#define ERR(e)                                                                                     \
    {                                                                                              \
        err = e;                                                                                   \
        break;                                                                                     \
    }

    bool completed = false;
    while (true) {
        // Seek to beginning of central directory
        //
        // We seek the file back until we reach the "End Of Central Directory"
        // signature "PK\5\6".
        //
        // end of central dir signature    4 bytes  (0x06054b50)
        // number of this disk             2 bytes   4
        // number of the disk with the
        // start of the central directory  2 bytes   6
        // total number of entries in the
        // central directory on this disk  2 bytes   8
        // total number of entries in
        // the central directory           2 bytes  10
        // size of the central directory   4 bytes  12
        // offset of start of central
        // directory with respect to
        // the starting disk number        4 bytes  16
        // .ZIP file comment length        2 bytes  20
        // --- SIZE UNTIL HERE: UNZIP_EOCD_SIZE ---
        // .ZIP file comment       (variable size)

        static constexpr const uint32_t fileCentralSize = 22;

        buffer_[fileCentralSize] = 0;

        if (!file_.seek(0, SSeekEnd)) {
            log_e("Unable to seek to end of file.");
            err = 0;
            break;
        }

        off_t length = file_.position();
        if (length < fileCentralSize) ERR(1);
        off_t ecdOffset = length - fileCentralSize;

        if (!file_.seek(ecdOffset, SSeekSet)) ERR(2);
        if (file_.read(buffer_, fileCentralSize) != fileCentralSize) ERR(3);
        if (getuint32(buffer_) != dirEndSignature) {
            // There must be a comment in the last entry. Search for the beginning of the entry
            off_t endOffset = ecdOffset - 65536;
            if (endOffset < 0) {
                endOffset = 0;
            }
            ecdOffset -= fileCentralSize;
            bool found = false;
            while (!found && (ecdOffset > endOffset)) {
                if (!file_.seek(ecdOffset, SSeekSet)) ERR(4);
                if (file_.read(buffer_, fileCentralSize + 5) != (fileCentralSize + 5)) ERR(5);
                auto p = (uint8_t *)memmem(buffer_, fileCentralSize + 5, "PK\5\6", 4);
                if (p != nullptr) {
                    ecdOffset += (p - buffer_);
                    if (!file_.seek(ecdOffset, SSeekSet)) ERR(6);
                    if (file_.read(buffer_, fileCentralSize) != fileCentralSize) ERR(7);
                    found = true;
                    break;
                }
                ecdOffset -= fileCentralSize;
            }
            if (!found) {
                ecdOffset = 0;
            }
        }

        if (ecdOffset > 0) {
            // Central Directory record structure:

            // [file header 1]
            // .
            // .
            // .
            // [file header n]
            // [digital signature] // PKZip 6.2 or later only

            uint32_t startOffset = getuint32(&buffer_[16]);
            uint16_t count = getuint16(&buffer_[10]);
            uint32_t length = ecdOffset - startOffset;

            if (count == 0) ERR(8);

            if (!file_.seek(startOffset, SSeekSet)) ERR(9);
            auto entries = SpiRamArrayPtr<uint8_t>(length);
            if (file_.read(entries.get(), length) != length) ERR(10);

            // fileEntries_.resize(count);
            // uint32_t fileEntryOffset = startOffset;
            uint32_t fileEntryOffset = 0;
            while (true) {
                DirFileHeader *dirFileHeader = (DirFileHeader *)&entries[fileEntryOffset];

                if (dirFileHeader->signature != dirFileHeaderSignature) {
                    if ((fileEntryOffset != length) || (count != 0))
                        ERR(11)
                    else {
                        completed = true;
                        break;
                    }
                }

                const char *fName = (char *)&entries[fileEntryOffset + sizeof(DirFileHeader)];
                std::string filePath = std::string(fName, dirFileHeader->filePathLength);

                std::shared_ptr<FileEntry> fe = std::make_shared<FileEntry>(
                    FileEntry{dirFileHeader->headerOffset, dirFileHeader->compressedSize,
                              dirFileHeader->uncompressedSize, dirFileHeader->compresionMethod});

                fileEntries_[filePath] = std::move(fe);

                fileEntryOffset += sizeof(DirFileHeader) + dirFileHeader->filePathLength +
                                   dirFileHeader->extraFieldLength +
                                   dirFileHeader->commentFieldLength;
                count--;
            }
        } else {
            log_e("Unable to read central directory.");
            ERR(14);
        }
        break;
    }

    if (!completed) {
        log_e("open error: %d", err);
        close();
    } else {
        // log_d("open completed!");
        showEntries();
    }

    return completed;
}

void Unzipper::close() {
    if (isOpen_) {
        if (currentFileEntry_ != nullptr) {
            currentFileEntry_.reset();
        };
        currentFileEntry_ = nullptr;

        fileEntries_.clear();
        if (file_) {
            file_.close();
        }
        isOpen_ = false;
    }
}

#if 0
/**
 * @brief Clean filePath path
 *
 * This function cleans filePath path that may contain relation folders (like '..').
 *
 * @param filePath The filePath to clean
 * @return char * The cleaned up filePath. Must be freed after usage.
 */

auto cleanFilePath(const char *filePath) -> char * {
    char *str = new char[strlen(filePath) + 1];

    const char *s = filePath;
    const char *u;
    char *t = str;

    while ((u = strstr(s, "/../")) != nullptr) {
        const char *ss = s; // keep it for copy in target
        s = u + 3;          // prepare for next iteration
        do {                // get rid of preceeding folder name
            u--;
        } while ((u > ss) && (*u != '/'));
        if (u >= ss) {
            while (ss != u)
                *t++ = *ss++;
        } else if ((*u != '/') && (t > str)) {
            do {
                t--;
            } while ((t > str) && (*t != '/'));
        }
    }
    if ((t == str) && (*s == '/')) s++;
    while ((*t++ = *s++))
        ;

    return str;
}
#endif

auto Unzipper::getFileSize() -> uint32_t {
    if (!isOpen_ || (currentFileEntry_ == nullptr)) {
        log_e("Hum... no current file!");
        return 0;
    }
    if constexpr (UNZIPPER_DEBUG) {
        log_d("File size: %" PRIu32, currentFileEntry_->size + 1);
    }
    return currentFileEntry_->size + 1;
}

auto Unzipper::fileExists(const std::string &filePath) -> bool {
    if (!isOpen_) {
        return false;
    }

    return fileEntries_.find(filePath) != fileEntries_.end();
}

void Unzipper::showEntries() {
    if constexpr (UNZIPPER_DEBUG) {
        std::cout << "---- Files available: ----" << std::endl;
        for (auto &f : fileEntries_) {
            std::cout << "pos: " << std::setw(7) << f.second->startPos
                      << " zip size: " << std::setw(7) << f.second->compressedSize
                      << " out size: " << std::setw(7) << f.second->size
                      << " method: " << std::setw(1) << f.second->method << " name: <" << f.first
                      << ">" << std::endl;
        }
        std::cout << "[End of List]" << std::endl;
    }
}

auto Unzipper::openFile(const std::string &filePath) -> bool {

    if (!isOpen_) {
        return false;
    }

    if (auto it = fileEntries_.find(filePath); it == fileEntries_.end()) {
        log_e("Unzipper openFile: File not found: <%s>", filePath.c_str());
        showEntries();
    } else {
        currentFileEntry_ = it->second;
        int err = 0;
        bool completed = false;

        while (true) {
            if (!file_.seek(currentFileEntry_->startPos, SSeekSet)) ERR(13);
            if (file_.read((uint8_t *)&fileHeader_, sizeof(FileHeader)) != sizeof(FileHeader))
                ERR(14);

            if (fileHeader_.signature != fileHeaderSignature) ERR(15);

            completed = true;
            break;
        }

        if (completed) {
            return true;
        } else {
            log_e("Unzipper openFile: Error!: %d", err);
        }
    }

    return false;
}

void Unzipper::closeFile() {
    if (currentFileEntry_ != nullptr) {
        currentFileEntry_.reset();
    }
    currentFileEntry_ = nullptr;
}

auto Unzipper::readFile(uint8_t *buffer, uint32_t bufferSize) -> uint32_t {
    // log_d("getFile: %s", filePath);

    if (!isOpen_ || (currentFileEntry_ == nullptr) ||
        ((currentFileEntry_->size + 1) != bufferSize)) {
        return 0;
    }

    int err = 0;
    uint32_t fileSize = 0;

    bool completed = false;
    while (true) {

        if (!file_.seek(currentFileEntry_->startPos + sizeof(FileHeader) +
                            fileHeader_.filePathLength + fileHeader_.extraFieldLength,
                        SSeekSet))
            ERR(17);

        if (buffer == nullptr) ERR(18);
        buffer[currentFileEntry_->size] = 0;

        if (currentFileEntry_->method == 0) { // No Compression
            if (file_.read((uint8_t *)buffer, currentFileEntry_->size) != currentFileEntry_->size)
                ERR(19);
        } else if (currentFileEntry_->method == 8) { // Deflate compression

            repeat = currentFileEntry_->compressedSize / BUFFER_SIZE;
            remains = currentFileEntry_->compressedSize % BUFFER_SIZE;
            current = 0;

            zstr_.zalloc = nullptr;
            zstr_.zfree = nullptr;
            zstr_.opaque = nullptr;
            zstr_.next_in = nullptr;
            zstr_.avail_in = 0;
            zstr_.avail_out = currentFileEntry_->size;
            zstr_.next_out = (unsigned char *)buffer;

            int zret = mz_inflateInit2(&zstr_, -15);

            // Use inflateInit2 with negative windowBits to get raw decompression
            if (zret != MZ_OK) {
                ERR(23);
            }

            // int szDecomp;

            // Decompress until deflate stream ends or end of file
            do {
                uint16_t size = current < repeat ? BUFFER_SIZE : remains;

                if constexpr (UNZIPPER_DEBUG) {
                    log_w("File size to read: %" PRIu16, size);
                }

                if (file_.read((uint8_t *)buffer_, size) != size) {
                    mz_inflateEnd(&zstr_);
                    err = 24;
                    goto error;
                }

                current++;

                zstr_.avail_in = size;
                zstr_.next_in = (unsigned char *)buffer_;

                zret = mz_inflate(&zstr_, MZ_NO_FLUSH);

                switch (zret) {
                case MZ_NEED_DICT:
                case MZ_DATA_ERROR:
                case MZ_MEM_ERROR:
                    mz_inflateEnd(&zstr_);
                    err = 25;
                    goto error;
                default:;
                }
            } while (current <= repeat);

            mz_inflateEnd(&zstr_);

        } else {
            break;
        }

        completed = true;
        break;
    }

error:
    if (!completed) {
        fileSize = 0;
        log_e("Unzipper readFile: Error!: %d", err);
    } else {
        fileSize = currentFileEntry_->size + 1;
    }

    return fileSize;
}
