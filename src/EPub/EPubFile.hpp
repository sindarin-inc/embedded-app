#include <utility>

#include "EPubOpf.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "Misc/ZipFile.hpp"
#include "Models/DocType.hpp"
#include "Renderers/RendererStream.hpp"
#include "pugixml.hpp"

class EPubFile : public RendererStream {
public:
    EPubFile(FileSystem &fs, const char *filename, const std::shared_ptr<Readable> &readable)
        : RendererStream(DocType::EPUB), epubZipFile_(fs, filename), readable_(readable),
          currentFileBuffer_(nullptr), currentFilePath_("") {
        open();
    }
    ~EPubFile() override { close(); };

    [[nodiscard]] auto read() -> int override { return 0; }
    [[nodiscard]] auto seek(uint32_t pos) -> bool override { return false; }
    [[nodiscard]] auto position() -> size_t override { return 0; }
    [[nodiscard]] auto size() -> size_t override { return 0; /* opf_->getTotalSize(); */ }

    // [[nodiscard]] inline auto getSpineIdxForOffset(uint32_t offset) const -> Idx {
    //     return opf_->getSpineIdxForOffset(offset);
    // }

    [[nodiscard]] inline auto getSpine(Idx idx) const -> const EPubOpf::SpineItem & {
        return opf_->getSpine(idx);
    }

    [[nodiscard]] auto findFileOffsetAtCharOffset(std::string path, uint32_t charOffset)
        -> uint32_t;

    [[nodiscard]] inline auto getSpineCount() const -> uint16_t { return opf_->getSpineCount(); }

    [[nodiscard]] auto getFile(std::string path) -> std::pair<std::unique_ptr<uint8_t[]>, uint32_t>;

    [[nodiscard]] inline auto getToc() const -> ReadableTableOfContentsChapterVector {
        return readable_->toc;
    }

    [[nodiscard]] auto getSpineIdx(const std::string &href) {
        uint16_t spineIdx = 0;
        while (spineIdx < opf_->getSpineCount()) {
            if (opf_->getSpine(spineIdx).item->href == href) {
                break;
            }
            spineIdx += 1;
        }
        return (spineIdx < opf_->getSpineCount()) ? spineIdx : 0;
    }

    [[nodiscard]] auto getXHTMLFile(std::string path) -> pugi::xml_document &;

    [[nodiscard]] inline auto getOffset(const pugi::char_t *item) const -> uint32_t {
        return item - currentFileBuffer_.get();
    }

    [[nodiscard]] inline auto getFullPath(std::string filename) const -> std::string {
        return opf_->getFullPath(std::move(filename));
    }

    [[nodiscard]] auto getUncompressedSize(Idx spineIdx) -> uint32_t {
        std::string filePath = opf_->getFullPath(opf_->getSpine(spineIdx).item->href);
        // log_w("Loading file %s", filePath.c_str());
        if (epubZipFile_.openFile(filePath.c_str()) == UNZ_OK) {
            unz_file_info info;
            epubZipFile_.getFileInfo(&info, nullptr, 0, nullptr, 0);
            epubZipFile_.closeFile();
            return info.uncompressed_size;
        }
        return 0;
    }

    pugi::xml_document currentFileDoc;

private:
    ZipFile epubZipFile_;
    const std::shared_ptr<Readable> &readable_;

    std::shared_ptr<EPubOpf> opf_;

    // std::unique_ptr<pugi::char_t[]> currentFileBuffer_;
    std::shared_ptr<pugi::char_t[]> currentFileBuffer_;
    std::string currentFilePath_;

    auto open() -> bool;
    void close();
};
