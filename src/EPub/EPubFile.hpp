#include "EPubOpf.hpp"
#include "Misc/ZipFile.hpp"
#include "Models/DocType.hpp"
#include "Renderers/RendererStream.hpp"
#include "pugixml.hpp"

class EPubFile : public RendererStream {
public:
    EPubFile(FileSystem &fs, const char *filename)
        : RendererStream(DocType::EPUB), epubZipFile_(fs, filename), currentFileBuffer_(nullptr),
          currentFilePath_("") {
        open();
    }
    ~EPubFile() override { close(); };

    [[nodiscard]] auto read() -> int override { return 0; }
    [[nodiscard]] auto seek(uint32_t pos) -> bool override { return false; }
    [[nodiscard]] auto position() -> size_t override { return 0; }
    [[nodiscard]] auto size() -> size_t override { return opf_->getTotalSize(); }

    [[nodiscard]] inline auto getSpineIdxForOffset(uint32_t offset) const -> Idx {
        return opf_->getSpineIdxForOffset(offset);
    }

    [[nodiscard]] inline auto getSpine(Idx idx) const -> const EPubOpf::SpineItem & {
        return opf_->getSpine(idx);
    }

    [[nodiscard]] inline auto getSpineCount() const -> uint16_t { return opf_->getSpineCount(); }

    [[nodiscard]] auto getFile(std::string path)
        -> std::tuple<std::unique_ptr<uint8_t[]>, uint32_t>;

    [[nodiscard]] auto getXHTMLFile(std::string path) -> pugi::xml_document &;

    [[nodiscard]] inline auto getOffset(const pugi::char_t *item) const -> uint32_t {
        return item - currentFileBuffer_;
    }

    [[nodiscard]] inline auto getFullPath(std::string filename) const -> std::string {
        return opf_->getFullPath(filename);
    }

    pugi::xml_document currentFileDoc_;

private:
    ZipFile epubZipFile_;
    std::shared_ptr<EPubOpf> opf_;

    pugi::char_t *currentFileBuffer_;
    std::string currentFilePath_;

    auto open() -> bool;
    void close();
};
