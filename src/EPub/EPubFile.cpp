#include "EPubFile.hpp"

#include "EPubMetaFile.hpp"

auto EPubFile::getXHTMLFile(std::string path) -> pugi::xml_document & {
    if (currentFilePath_ != path) {
        if (epubZipFile_.openFile(path.c_str()) != UNZ_OK) {
            log_e("Failed to open XHTML file %s", path.c_str());
        } else {
            unz_file_info info;
            epubZipFile_.getFileInfo(&info, nullptr, 0, nullptr, 0);

            if (currentFileBuffer_ != nullptr) {
                delete[] currentFileBuffer_;
            }
            currentFileBuffer_ = new pugi::char_t[info.uncompressed_size];
            if (currentFileBuffer_ != nullptr) {
                size_t len = epubZipFile_.readFromFile((uint8_t *)currentFileBuffer_,
                                                       info.uncompressed_size);
                epubZipFile_.closeFile();

                if (len == info.uncompressed_size) {
                    pugi::xml_document doc;
                    pugi::xml_parse_result result = doc.load_string(currentFileBuffer_);

                    if (result) {
                        currentFilePath_ = path;
                    } else {
                        log_e("Failed to parse XHTML file %s: %s", path.c_str(),
                              result.description());
                    }
                } else {
                    log_e("Unable to unzip file %s", path.c_str());
                }
            } else {
                log_e("Unable to allocate space for file %s, space required: %zu", path.c_str(),
                      info.uncompressed_size);
            }
        }
    }

    return currentFileDoc_;
}

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
