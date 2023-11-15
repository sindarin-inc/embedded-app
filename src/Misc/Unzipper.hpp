#pragma once

#include <cinttypes>
#include <cstring>
#include <memory>
#include <miniz/miniz.h>
#include <string>
#include <unordered_map>

#include "Storage/FileSystem.hpp"

static const constexpr bool UNZIPPER_DEBUG = false;

class Unzipper {
private:
    static constexpr char const *TAG = "Unzipper";

#pragma pack(push, 1)
    // File header:

    // central file header signature   4 bytes  (0x02014b50)
    // version made by                 2 bytes
    // version needed to extract       2 bytes
    // general purpose bit flag        2 bytes
    // compression method              2 bytes
    // last mod file time              2 bytes
    // last mod file date              2 bytes
    // crc-32                          4 bytes
    // compressed size                 4 bytes
    // uncompressed size               4 bytes
    // file name length                2 bytes
    // extra field length              2 bytes
    // file comment length             2 bytes
    // disk number start               2 bytes
    // internal file attributes        2 bytes
    // external file attributes        4 bytes
    // relative offset of local header 4 bytes

    // file name (variable size)
    // extra field (variable size)
    // file comment (variable size)
    struct DirFileHeader {
        uint32_t signature;
        uint16_t version;
        uint16_t extractVersion;
        uint16_t flags;
        uint16_t compresionMethod;
        uint16_t lastModTime;
        uint16_t lastModDate;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint16_t filePathLength;
        uint16_t extraFieldLength;
        uint16_t commentFieldLength;
        uint16_t diskNumberStart;
        uint16_t internalFileAttr;
        uint32_t externalFileAttr;
        uint32_t headerOffset;
    };

    // Local header record.

    // local file header signature     4 bytes  (0x04034b50)
    // version needed to extract       2 bytes
    // general purpose bit flag        2 bytes
    // compression method              2 bytes
    // last mod file time              2 bytes
    // last mod file date              2 bytes
    // crc-32                          4 bytes
    // compressed size                 4 bytes
    // uncompressed size               4 bytes
    // file name length                2 bytes
    // extra field length              2 bytes

    // file name (variable size)
    // extra field (variable size)
    struct FileHeader {
        uint32_t signature;
        uint16_t extractVersion;
        uint16_t flags;
        uint16_t compressionMethod;
        uint16_t lastModTime;
        uint16_t lastModDate;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint16_t filePathLength;
        uint16_t extraFieldLength;
    } fileHeader_;
#pragma pack(pop)

    static constexpr const uint32_t DIR_FILE_HEADER_SIGNATURE = 0x02014b50;
    static constexpr const uint32_t FILE_HEADER_SIGNATURE = 0x04034b50U;
    static constexpr const uint32_t DIR_END_SIGNATURE = 0x06054b50;

    static const int BUFFER_SIZE = 1024 * 16;
    uint8_t buffer_[BUFFER_SIZE];

    struct FileEntry {
        uint32_t startPos;       // in zip file
        uint32_t compressedSize; // in zip file
        uint32_t size;           // once decompressed
        uint16_t method;         // compress method (0 = not compressed, 8 = DEFLATE)
    };

    typedef SpiramMap<SpiramString, std::shared_ptr<FileEntry>> FileEntries;

    FileEntries fileEntries_;
    std::shared_ptr<FileEntry> currentFileEntry_;

    // As long as we are operating as little endian, the following is ok
    [[nodiscard]] inline auto getuint32(const uint8_t *b) const -> uint32_t {
        return *(uint32_t *)b;
    }
    [[nodiscard]] inline auto getuint16(const uint8_t *b) const -> uint16_t {
        return *(uint16_t *)b;
    }

    auto cleanFilePath(const char *filePath) -> std::string;

    uint16_t repeat_;
    uint16_t remains_;
    uint16_t current_;

    SFile file_; // Current File Descriptor
    bool isOpen_{false};
    FileSystem &fileSystem_;
    std::string filePath_;

    mz_stream zstr_;

public:
    Unzipper(FileSystem &fs, const char *filePath) : fileSystem_(fs), filePath_(filePath) {}
    ~Unzipper() {
        if (isOpen_) {
            close();
        }
    }
    auto open() -> bool;
    void close();

    auto getFileSize() -> uint32_t;
    auto readFile(uint8_t *fileData, uint32_t fileDataSize) -> uint32_t;
    auto fileExists(const std::string &filePath) -> bool;
    void showEntries();
    auto openFile(const std::string &filePath) -> bool;
    void closeFile();
};
