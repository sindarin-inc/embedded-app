#include "EPubFile.hpp"

#include <expat.h>

#include "EPubMetaFile.hpp"

auto EPubFile::open() -> bool {
    if (!epubZipFile_.open()) {
        log_e("Failed to open ePub file");
        return false;
    }

    EPubMetaFile manifest(epubZipFile_, "META-INF/container.xml");
    manifest.parse();
    for (auto &path : manifest.rootfilePaths) {
        log_i("Rootfile path: %s", path.c_str());
    }

    if (manifest.rootfilePaths.size() == 0) {
        log_e("No rootfile paths found");
        return false;
    } else if (manifest.rootfilePaths.size() > 1) {
        log_w("Multiple rootfile paths found");
    }

    if (!EPubOpf(epubZipFile_, manifest.rootfilePaths[0].c_str()).parse(opfData_)) {
        log_e("Unable to complete the opf file parsing!");
        return false;
    }

#if 0
    // To stream out data, build a list of each manifest file of type application/xhtml+xml
    xhtmlItems_.clear();
    for (const auto &item : data.manifest) {
        if (item.mediaType == "application/xhtml+xml") {
            xhtmlItems_.push_back(item);
        }
    }

    // Now let's grab the size of each file
    int totalSize = 0;
    for (auto &item : xhtmlItems_) {
        epubZipFile_.openFile(item.fullPath().c_str());
        unz_file_info info;
        epubZipFile_.getFileInfo(&info, nullptr, 0, nullptr, 0);
        item.uncompressedSize = info.uncompressed_size;
        totalSize += info.uncompressed_size;
        epubZipFile_.closeFile();
    }

    contentLength_ = totalSize;
    log_d("Total size of all ePub text is %d", contentLength_);

    // Open the first file
    epubZipFile_.openFile(xhtmlItems_[0].fullPath().c_str());
#endif

    return true;
}

void EPubFile::close() { return epubZipFile_.close(); }

auto EPubFile::readBytes(uint8_t *buf, int size) -> int {
    // While we have bytes to read, read them!
    size_t len = epubZipFile_.readFromFile(buf, size);

    if (len < size) {
        epubZipFile_.closeFile();

        currentXhtmlIndex_++;
        // If there's another file to open
        // if (currentXhtmlIndex_ < xhtmlItems_.size()) {
        //     // Open the next file
        //     auto &item = xhtmlItems_[currentXhtmlIndex_];
        //     log_d("Opening %s", item.fullPath().c_str());
        //     epubZipFile_.openFile(item.fullPath().c_str());
        //     // Read the bytes
        //     size_t secondLen = epubZipFile_.readFromFile(buf + len, size - len);
        //     len += secondLen;
        // } else {
        //     log_i("No more files to read");
        // }
    }

    position_ += len;
    // We're done!
    return len;
};

// TODO: Maybe this could be the default implememtation of readBytes?
auto EPubFile::read() -> int {
    int out;
    readBytes((uint8_t *)&out, 1);
    return out;
};

auto EPubFile::seek(uint32_t pos) -> bool {
    if (pos < position_) {
        log_w("Seeking backwards in ePub from %d to %" PRIu32 "", position_, pos);
        position_ = 0;
        currentXhtmlIndex_ = 0;
    //     epubZipFile_.closeFile();
    //     epubZipFile_.openFile(xhtmlItems_[0].fullPath().c_str());
    }

    // Seek forward to position
    while (position_ < pos && position_ < contentLength_) {
        // Read a byte at a time until we get to the position
        read();
    }
    return true;
};
