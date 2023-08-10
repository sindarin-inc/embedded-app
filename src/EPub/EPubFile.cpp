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

    opf_ = std::make_shared<EPubOpf>(epubZipFile_, manifest.rootfilePaths[0].c_str());
    opf_->parse();

    log_d("Total size of all ePub text is %d", opf_->getTotalSize());

    // Open the first file
    // epubZipFile_.openFile(xhtmlItems_[0].fullPath().c_str());

    return true;
}

void EPubFile::close() { return epubZipFile_.close(); }
