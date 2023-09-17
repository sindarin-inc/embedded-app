#include "EPubFile.hpp"

#include <utility>

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
}

auto EPubFile::getXHTMLFile(const std::string &path) -> pugi::xml_document & {
    std::string filePath = opf_->getFullPath(path);
    // log_w("Loading file %s", filePath.c_str());
    if (currentFilePath_ != filePath) {

        log_w("currentFilePath_: %s, filePath : %s", currentFilePath_.c_str(), filePath.c_str());

        PROFILE_START(OpenXHTMLFile);
        auto res = epubZipFile_.openFile(filePath.c_str());
        PROFILE_END_LOG_MS(OpenXHTMLFile);

        if (res != UNZ_OK) {
            log_e("Failed to open XHTML file %s", filePath.c_str());
        } else {
            unz_file_info info;

            PROFILE_START(getFileInfo);
            epubZipFile_.getFileInfo(&info, nullptr, 0, nullptr, 0);
            PROFILE_END_LOG_MS(getFileInfo);

            currentFileBuffer_ = SpiRamArrayPtr<pugi::char_t>(info.uncompressed_size + 1);
            // currentFileBuffer_ = std::make_unique<pugi::char_t[]>(info.uncompressed_size);

            if (currentFileBuffer_ != nullptr) {
                PROFILE_START(ReadXHTMLFromEPub);
                size_t len = epubZipFile_.readFromFile((uint8_t *)currentFileBuffer_.get(),
                                                       info.uncompressed_size);
                epubZipFile_.closeFile();
                PROFILE_END_LOG_MS(ReadXHTMLFromEPub);

                currentFileBuffer_[len] = '\0';

                if (len == info.uncompressed_size) {
                    // Using load_buffer_inplace() is mandatory here as it permits the
                    // computation of a node offset location, as string elements are taken
                    // directly in the currentFileBuffer instead of being copied.
                    // (see the EPubFile::getOffset() method).

                    PROFILE_START(ParseXHTML);
                    pugi::xml_parse_result result =
                        currentFileDoc.load_buffer_inplace(currentFileBuffer_.get(), len);
                    PROFILE_END_LOG_MS(ParseXHTML);

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

    return currentFileDoc;
}

auto EPubFile::open() -> bool {
    EPUB_PROFILE_START(openEPubZipFile);
    if (!epubZipFile_.open()) {
        log_e("Failed to open ePub file");
        return false;
    }
    EPUB_PROFILE_END(openEPubZipFile);

    EPUB_PROFILE_START(EPubManifest);
    EPubMetaFile manifest(epubZipFile_, "META-INF/container.xml");
    manifest.parse();
    EPUB_PROFILE_END(EPubManifest);

    // for (auto &path : manifest.rootfilePaths) {
    //     log_i("Rootfile path: %s", path.c_str());
    // }

    if (manifest.rootfilePaths.size() == 0) {
        log_e("No rootfile paths found");
        return false;
    } else if (manifest.rootfilePaths.size() > 1) {
        log_w("Multiple rootfile paths found");
    }

    EPUB_PROFILE_START(EPubOpf)
    opf_ = std::make_shared<EPubOpf>(epubZipFile_, manifest.rootfilePaths[0].c_str());
    opf_->parse();
    EPUB_PROFILE_END(EPubOpf)

    // log_d("Total size of all ePub text is %d", opf_->getTotalSize());
    return true;
}

void EPubFile::close() { return epubZipFile_.close(); }
