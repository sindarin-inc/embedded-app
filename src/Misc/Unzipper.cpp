#include "Unzipper.hpp"

#include <iomanip>
#include <iostream>

#include "Misc/SpiramAllocator.hpp"

auto Unzipper::open() -> bool {
    if (!file_) {
        log_e("Unzipper has no file");
        return false;
    }

    isOpen_ = true;

    int err = 99;

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
        if (getuint32(buffer_) != DIR_END_SIGNATURE) {
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
                auto p = static_cast<uint8_t *>(memmem(buffer_, fileCentralSize + 5, "PK\5\6", 4));
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
            if ((entries == nullptr) || (entries == nullptr)) ERR(10);
            if (file_.read(entries.get(), length) != length) ERR(11);

            // fileEntries_.resize(count);
            // uint32_t fileEntryOffset = startOffset;
            uint32_t fileEntryOffset = 0;
            while (count > 0) {
                auto *dirFileHeader = reinterpret_cast<DirFileHeader *>(&entries[fileEntryOffset]);

                if (dirFileHeader->signature != DIR_FILE_HEADER_SIGNATURE) {
                    if (fileEntryOffset != length) ERR(11);
                }

                const char *fName =
                    reinterpret_cast<char *>(&entries[fileEntryOffset + sizeof(DirFileHeader)]);
                auto filePath = SpiramString(fName, dirFileHeader->filePathLength);

                std::shared_ptr<FileEntry> fe = std::make_shared<FileEntry>(
                    FileEntry{dirFileHeader->headerOffset, dirFileHeader->compressedSize,
                              dirFileHeader->uncompressedSize, dirFileHeader->compresionMethod});

                fileEntries_[filePath] = std::move(fe);

                fileEntryOffset += sizeof(DirFileHeader) + dirFileHeader->filePathLength +
                                   dirFileHeader->extraFieldLength +
                                   dirFileHeader->commentFieldLength;
                count--;
            }
            completed = count == 0;
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
struct dir_component {
    char * dir;         // one component of the path (no separators)
    int    len;         // length of this component
};

// a way to print struct dir_component entries for debug
static char lbuf[32];
char *
dc_str(struct dir_component * dir)
{
    int len = dir->len;

    if (len > sizeof(lbuf)-1)
        len = sizeof(lbuf)-1;

    if (len)
        strncpy(lbuf, dir->dir, len);
    lbuf[len] = '\0';

    return lbuf;
}


char *
canonicalize_path(char * path, int debug)
{
    struct dir_component * dirs;
    struct dir_component * dp;
    char * p;
    char * q;
    int    n_dirs;
    int    first_dir;
    int    i;
    int    j;

    if (path[0] == '\0')
        return path;

    //
    // estimate the number of directory components in path
    //

    n_dirs = 1;
    for (p = path; *p; p++)
        if (*p == '/')
            n_dirs++;

    if (debug) {
        printf("========\n");
        printf("in: %s (%d)\n", path, n_dirs);
    }

    dirs = malloc(sizeof(struct dir_component) * n_dirs);
    if (dirs == NULL) {
        printf("canonicalize_path: out of memory\n");
        return NULL;
    }

    //
    // break path into component parts
    //
    // path separators mark the start of each directory component
    // (except maybe the first)
    //

    n_dirs = 0;
    dirs[n_dirs].dir = path;
    dirs[n_dirs].len = 0;

    for (p = path; *p; p++) {
        if (*p == '/') {
            // found a new component, save its location
            // and start counting its length
            n_dirs++;
            dirs[n_dirs].dir = p + 1;
            dirs[n_dirs].len = 0;
        } else {
            dirs[n_dirs].len++;
        }
    }
    n_dirs++;

    if (debug) {
        for (i = 0; i < n_dirs; i++)
            printf(" %2d %s\n", dirs[i].len, dc_str(&dirs[i]));
    }

    //
    // canonicalize '.' and '..'
    //

    for (i = 0; i < n_dirs; i++) {
        dp = &dirs[i];

        // mark '.' as an empty component
        if (dp->len == 1  &&  dp->dir[0] == '.') {
            dirs[i].len = 0;
            continue;
        }

        // '..' eliminates the previous non-empty component
        if (dp->len == 2  &&  dp->dir[0] == '.'  &&  dp->dir[1] == '.') {
            dirs[i].len = 0;
            for (j = i - 1; j >= 0; j--)
                if (dirs[j].len > 0)
                    break;
            if (j < 0) {
                free(dirs);     // no previous components are available,
                return NULL;    // the path is invalid
            }
            dirs[j].len = 0;
        }
    }

    if (debug) {
        printf("----\n");
        for (i = 0; i < n_dirs; i++)
            printf(" %2d %s\n", dirs[i].len, dc_str(&dirs[i]));
    }

    //
    // reconstruct path using non-zero length components
    //

    p = path;
    if (*p == '/')              // if path was absolute, keep it that way
        p++;
    first_dir = 1;
    for (i = 0; i < n_dirs; i++)
        if (dirs[i].len > 0) {
            if (!first_dir)
                *p++ = '/';
            q = dirs[i].dir;
            j = dirs[i].len;
            while (j--)
                *p++ = *q++;
            first_dir = 0;
        }
    *p = '\0';

    free(dirs);
    return path;
}
#endif

// This method cleans filePath path that may contain relation folders (like '..').
auto Unzipper::cleanFilePath(const char *filePath) -> std::string {
    std::string ret(strlen(filePath), '\0');

    char *str = ret.data();

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
            while (ss != u) {
                *t++ = *ss++;
            }
        } else if ((*u != '/') && (t > str)) {
            do {
                t--;
            } while ((t > str) && (*t != '/'));
        }
    }
    if ((t == str) && (*s == '/')) {
        s++;
    }
    while ((*t++ = *s++)) {
        ;
    }

    ret.resize(strlen(ret.data()));
    return ret;
}

auto Unzipper::getFileSize() -> uint32_t {
    if (!isOpen_ || (currentFileEntry_ == nullptr)) {
        log_e("Hum... no current file!");
        return 0;
    }
    if constexpr (UNZIPPER_DEBUG) {
        log_d("File size: %" PRIu32, currentFileEntry_->size);
    }
    return currentFileEntry_->size;
}

auto Unzipper::fileExists(const std::string &filePath) -> bool {
    if (!isOpen_) {
        return false;
    }

    std::string cleanedFilePath = cleanFilePath(filePath.c_str());

    return fileEntries_.find(static_cast<SpiramString>(cleanedFilePath)) != fileEntries_.end();
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

    std::string cleanedFilePath = cleanFilePath(filePath.c_str());

    if (auto it = fileEntries_.find(static_cast<SpiramString>(cleanedFilePath));
        it == fileEntries_.end()) {
        log_e("Unzipper openFile: File not found: <%s>", cleanedFilePath.c_str());
        showEntries();
    } else {
        currentFileEntry_ = it->second;
        int err = 0;
        bool completed = false;

        while (true) {
            if (!file_.seek(currentFileEntry_->startPos, SSeekSet)) ERR(13);
            if (file_.read(reinterpret_cast<uint8_t *>(&fileHeader_), sizeof(FileHeader)) !=
                sizeof(FileHeader))
                ERR(14);

            if (fileHeader_.signature != FILE_HEADER_SIGNATURE) ERR(15);

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

auto Unzipper::readFile(uint8_t *fileData, uint32_t fileDataSize) -> uint32_t {
    // log_d("getFile: %s", filePath);

    if (!isOpen_ || (currentFileEntry_ == nullptr) || (currentFileEntry_->size > fileDataSize)) {
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

        if (fileData == nullptr) ERR(18);

        if (currentFileEntry_->method == 0) { // No Compression
            if (file_.read(fileData, currentFileEntry_->size) != currentFileEntry_->size) ERR(19);
        } else if (currentFileEntry_->method == 8) { // Deflate compression

            repeat_ = currentFileEntry_->compressedSize / BUFFER_SIZE;
            remains_ = currentFileEntry_->compressedSize % BUFFER_SIZE;
            current_ = 0;

            zstr_.zalloc = SpiZAlloc;
            zstr_.zfree = SpiZFree;
            // zstr_.zalloc = nullptr;
            // zstr_.zfree = nullptr;
            zstr_.opaque = nullptr;
            zstr_.next_in = nullptr;
            zstr_.avail_in = 0;
            zstr_.avail_out = currentFileEntry_->size;
            zstr_.next_out = static_cast<unsigned char *>(fileData);

            int zret = mz_inflateInit2(&zstr_, -15);

            // Use inflateInit2 with negative windowBits to get raw decompression
            if (zret != MZ_OK) {
                ERR(23);
            }

            // int szDecomp;

            // Decompress until deflate stream ends or end of file
            do {
                uint16_t size = current_ < repeat_ ? BUFFER_SIZE : remains_;

                if constexpr (UNZIPPER_DEBUG) {
                    log_w("File size to read: %" PRIu16, size);
                }

                if (file_.read(static_cast<uint8_t *>(buffer_), size) != size) {
                    mz_inflateEnd(&zstr_);
                    err = 24;
                    goto error;
                }

                current_++;

                zstr_.avail_in = size;
                zstr_.next_in = static_cast<unsigned char *>(buffer_);

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
            } while (current_ <= repeat_);

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
        fileSize = currentFileEntry_->size;
    }

    return fileSize;
}
