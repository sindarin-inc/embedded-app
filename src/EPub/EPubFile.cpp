#include "EPubFile.hpp"

#include "EPubMetaFile.hpp"

auto EPubFile::findFileOffsetAtCharOffset(std::string path, uint32_t charOffset) -> uint32_t {

    pugi::xml_document &doc = getXHTMLFile(path);

    struct Walker : pugi::xml_tree_walker {
        virtual bool for_each(pugi::xml_node &node) {
            if (node.type() == pugi::xml_node_type::node_pcdata) {
                uint32_t length = strlen(node.value());
                if ((chOffset + length) > charOffset) {
                    offset = epub->getOffset(node.value()) + (charOffset - chOffset);
                    return false;
                }
                chOffset += length;
            }
            return true;
        }

        uint32_t offset;
        uint32_t chOffset;
        uint32_t charOffset;
        EPubFile *epub;
    } walker;

    walker.offset = walker.chOffset = 0;
    walker.charOffset = charOffset;
    walker.epub = this;

    doc.child("html").child("body").traverse(walker);
    return walker.offset;

    return 0;
}

// Returns a pointer to the buffer containing the extracted file from the EPub and it's length in a
// tuple
auto EPubFile::getFile(std::string path) -> std::tuple<std::unique_ptr<uint8_t[]>, uint32_t> {

    std::unique_ptr<uint8_t[]> buffer = nullptr;

    std::string filePath = opf_->getFullPath(path);

    if (epubZipFile_.openFile(filePath.c_str()) != UNZ_OK) {
        log_e("Failed to open XHTML file %s", filePath.c_str());
    } else {
        unz_file_info info;
        epubZipFile_.getFileInfo(&info, nullptr, 0, nullptr, 0);

        buffer = std::make_unique<uint8_t[]>(info.uncompressed_size);
        if (buffer != nullptr) {
            size_t len = epubZipFile_.readFromFile(buffer.get(), info.uncompressed_size);
            epubZipFile_.closeFile();
            if (len != info.uncompressed_size) {
                log_e("Unable to unzip file %s", filePath.c_str());
            }
            uint32_t length = info.uncompressed_size;
            return {std::move(buffer), length};
        }
    }

    return {nullptr, 0};
}

auto EPubFile::getXHTMLFile(std::string path) -> pugi::xml_document & {
    std::string filePath = opf_->getFullPath(path);
    // log_w("Loading file %s", filePath.c_str());
    if (currentFilePath_ != filePath) {
        if (epubZipFile_.openFile(filePath.c_str()) != UNZ_OK) {
            log_e("Failed to open XHTML file %s", filePath.c_str());
        } else {
            unz_file_info info;
            epubZipFile_.getFileInfo(&info, nullptr, 0, nullptr, 0);

            currentFileBuffer_ = SpiRamArrayPtr<pugi::char_t>(info.uncompressed_size);
            // currentFileBuffer_ = std::make_unique<pugi::char_t[]>(info.uncompressed_size);

            if (currentFileBuffer_ != nullptr) {
                size_t len = epubZipFile_.readFromFile((uint8_t *)currentFileBuffer_.get(),
                                                       info.uncompressed_size);
                epubZipFile_.closeFile();

                if (len == info.uncompressed_size) {
                    // Using load_buffer_inplace() is mandatory here as it permits the
                    // computation of a node offset location, as string elements are taken
                    // directly in the currentFileBuffer instead of being copied.
                    // (see the EPubFile::getOffset() method).
                    pugi::xml_parse_result result =
                        currentFileDoc_.load_buffer_inplace(currentFileBuffer_.get(), len);

                    if (result) {
                        currentFilePath_ = filePath;
                    } else {
                        log_e("Failed to parse XHTML file %s: %s", filePath.c_str(),
                              result.description());
                    }
                } else {
                    log_e("Unable to unzip file %s", filePath.c_str());
                }
            } else {
                log_e("Unable to allocate space for file %s, space required: %" PRIu32,
                      filePath.c_str(), static_cast<uint32_t>(info.uncompressed_size));
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
    // for (auto &path : manifest.rootfilePaths) {
    //     log_i("Rootfile path: %s", path.c_str());
    // }

    if (manifest.rootfilePaths.size() == 0) {
        log_e("No rootfile paths found");
        return false;
    } else if (manifest.rootfilePaths.size() > 1) {
        log_w("Multiple rootfile paths found");
    }

    opf_ = std::make_shared<EPubOpf>(epubZipFile_, manifest.rootfilePaths[0].c_str());
    opf_->parse();

    // log_d("Total size of all ePub text is %d", opf_->getTotalSize());

    // Open the first file
    // epubZipFile_.openFile(xhtmlItems_[0].fullPath().c_str());

    return true;
}

void EPubFile::close() { return epubZipFile_.close(); }
