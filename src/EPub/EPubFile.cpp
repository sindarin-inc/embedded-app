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

    auto data = EPubOpf(epubZipFile_, manifest.rootfilePaths[0].c_str()).parse();

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

    return true;
}

void EPubFile::close() { return epubZipFile_.close(); }
