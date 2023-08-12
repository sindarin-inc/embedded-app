#include "EPubOpf.hpp"
#include "Misc/ZipFile.hpp"
#include "Renderers/RendererStream.hpp"
#include "pugixml.hpp"

class EPubFile : public RendererStream {
public:
    EPubFile(FileSystem &fs, const char *filename)
        : RendererStream(StreamType::EPUB), epubZipFile_(fs, filename), currentFilePath_(""),
          currentFileBuffer_(nullptr) {
        open();
    }
    ~EPubFile() override { close(); };

    [[nodiscard]] auto read() -> int override { return 0; }
    [[nodiscard]] auto seek(uint32_t pos) -> bool override { return false; }
    [[nodiscard]] auto position() -> size_t override { return 0; }
    [[nodiscard]] auto size() -> size_t override { return opf_->getTotalSize(); }

    [[nodiscard]] inline auto getSpineIdxForOffset(uint32_t offset) const -> const uint16_t {
        return opf_->getSpineIdxForOffset(offset);
    }

    [[nodiscard]] inline auto getSpine(uint16_t idx) const -> const EPubOpf::SpineItem & {
        return opf_->getSpine(idx);
    }

    [[nodiscard]] auto getXHTMLFile(std::string path) -> pugi::xml_document &;

    [[nodiscard]] inline auto getOffset(const pugi::char_t *item) const -> size_t {
        return item - currentFileBuffer_;
    }

private:
    ZipFile epubZipFile_;
    std::shared_ptr<EPubOpf> opf_;

    pugi::char_t *currentFileBuffer_;
    pugi::xml_document currentFileDoc_;
    std::string currentFilePath_;

    auto open() -> bool;
    void close();
};
