#include "EPubFile.hpp"

#include "EPubDefs.hpp"
#include "EPubMetaFile.hpp"

auto EPubFile::findFileOffsetAtCharOffset(const std::string &path, uint32_t charOffset)
    -> uint32_t {

    pugi::xml_document &doc = getXHTMLFile(path);

    struct Walker : pugi::xml_tree_walker {
        auto for_each(pugi::xml_node &node) -> bool override {
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
auto EPubFile::getFile(const std::string &completeFilePath)
    -> std::pair<std::shared_ptr<uint8_t[]>, uint32_t> {

    std::shared_ptr<uint8_t[]> buffer = nullptr;

    EPUB_PROFILE_START(UnzipperOpenFile);
    auto res = epubUnzipper_.openFile(completeFilePath);
    EPUB_PROFILE_END(UnzipperOpenFile);

    if (!res) {
        log_e("Unzipper Failed to open file %s", completeFilePath.c_str());
    } else {
        EPUB_PROFILE_START(UnzipperGetFileSize);
        auto size = epubUnzipper_.getFileSize();
        EPUB_PROFILE_END(UnzipperGetFileSize);

        if (size > 0) {
            buffer = SpiRamArrayPtr<uint8_t>(size);

            if (buffer != nullptr) {
                EPUB_PROFILE_START(UnzipperReadFromFile);
                uint32_t length = epubUnzipper_.readFile(buffer.get(), size);
                epubUnzipper_.closeFile();
                EPUB_PROFILE_END(UnzipperReadFromFile);
                if (length == size) {
                    return {std::move(buffer), length};
                }
            } else {
                epubUnzipper_.closeFile();
                log_e("Unable to allocate space for file %s, space required: %" PRIu32,
                      completeFilePath.c_str(), static_cast<uint32_t>(size));
            }
        }
    }

    return {nullptr, 0};

#if 0
    if (epubZipFile_.openFile(completeFilePath.c_str()) != UNZ_OK) {
        log_e("Failed to open XHTML file %s", completeFilePath.c_str());
    } else {
        unz_file_info info;
        epubZipFile_.getFileInfo(&info, nullptr, 0, nullptr, 0);

        buffer = SpiRamArrayPtr<uint8_t>(info.uncompressed_size + 1);

        if (buffer != nullptr) {
            size_t len = epubZipFile_.readFromFile(buffer.get(), info.uncompressed_size);
            epubZipFile_.closeFile();

            if (len != info.uncompressed_size) {
                log_e("Unable to unzip file %s", completeFilePath.c_str());
            }
            uint32_t length = info.uncompressed_size + 1;
            buffer[len] = '\0';
            return {std::move(buffer), length};
        }
    }

    return {nullptr, 0};
#endif
}

auto EPubFile::getXHTMLFile(const std::string &path) -> pugi::xml_document & {
    std::string filePath = opf_->getFullPath(path);
    // log_w("Loading file %s", filePath.c_str());
    uint32_t length;

    if (currentFilePath_ != filePath) {

        // log_w("currentFilePath_: %s, filePath : %s", currentFilePath_.c_str(), filePath.c_str());

        std::tie(currentFileBuffer_, length) = getFile(filePath);

        if (currentFileBuffer_ != nullptr) {
            // Using load_buffer_inplace() is mandatory here as it permits the
            // computation of a node offset location, as string elements are taken
            // directly in the currentFileBuffer instead of being copied.
            // (see the EPubFile::getOffset() method).

            EPUB_PROFILE_START(ParseXHTML);
            pugi::xml_parse_result result = currentFileDoc.load_buffer_inplace(
                currentFileBuffer_.get(), length,
                pugi::parse_default | pugi::parse_ws_pcdata);
            EPUB_PROFILE_END(ParseXHTML);

            if (result) {
                currentFilePath_ = filePath;
            } else {
                log_e("Failed to parse XHTML file %s: %s", filePath.c_str(), result.description());
            }
        } else {
            log_e("Unable to unzip file %s", filePath.c_str());
        }
    }

    return currentFileDoc;
}

auto EPubFile::open() -> bool {

    PROFILE_START(openEPub);

    EPUB_PROFILE_START(UnzipperOpen);
    if (!epubUnzipper_.open()) {
        log_e("Failed to open ePub file");
        return false;
    }
    EPUB_PROFILE_END(UnzipperOpen);

    EPUB_PROFILE_START(EPubManifest);
    EPubMetaFile manifest("META-INF/container.xml");
    auto [manifestBuffer, manifestLength] = getFile("META-INF/container.xml");
    if (manifestBuffer == nullptr) {
        log_e("Failed to open ePub's manifest %s", "META-INF/container.xml");
        return false;
    } else {
        manifest.parse(manifestBuffer, manifestLength);
    }
    EPUB_PROFILE_END(EPubManifest);

    // for (auto &path : manifest.rootfilePaths) {
    //     log_i("Rootfile path: %s", path.c_str());
    // }

    if (manifest.rootfilePaths_.size() == 0) {
        log_e("No rootfile paths found");
        return false;
    } else if (manifest.rootfilePaths_.size() > 1) {
        log_w("Multiple rootfile paths found");
    }

    EPUB_PROFILE_START(EPubOpf)
    auto [opfBuffer, length] = getFile(manifest.rootfilePaths_[0].c_str());
    if (opfBuffer == nullptr) {
        log_e("Failed to open ePub's opf %s", manifest.rootfilePaths_[0].c_str());
        return false;
    } else {
        opf_ = std::make_shared<EPubOpf>(manifest.rootfilePaths_[0].c_str());
        opf_->parse(opfBuffer, length);
    }
    EPUB_PROFILE_END(EPubOpf)

    PROFILE_END_LOG_MS(openEPub);

    // log_d("Total size of all ePub text is %d", opf_->getTotalSize());
    return true;
}

void EPubFile::close() { epubUnzipper_.close(); }
