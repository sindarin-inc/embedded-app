#pragma once

#include <pugixml.hpp>
#include <utility>

#include "EPubDefs.hpp"
#include "EPubOpf.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "Misc/Unzipper.hpp"
#include "Models/DocType.hpp"
#include "Renderers/RendererStream.hpp"

class EPubFile : public RendererStream {
public:
    EPubFile(FileSystem &fs, const char *filename, std::shared_ptr<Readable> readable)
        : RendererStream(DocType::EPUB), epubUnzipper_(fs, filename),
          readable_(std::move(readable)), currentFileBuffer_(nullptr), currentFilePath_("") {
        open();
    }
    ~EPubFile() override { close(); };

    [[nodiscard]] auto read() -> int override { return 0; }
    [[nodiscard]] auto seek(uint32_t pos) -> bool override { return false; }
    [[nodiscard]] auto position() -> size_t override { return 0; }
    [[nodiscard]] auto size() -> size_t override { return 0; /* opf_->getTotalSize(); */ }

    [[nodiscard]] auto isAnArticle() const -> bool { return readable_->isArticle(); }

    // [[nodiscard]] inline auto getSpineIdxForOffset(uint32_t offset) const -> Idx {
    //     return opf_->getSpineIdxForOffset(offset);
    // }

    [[nodiscard]] inline auto getSpine(Idx idx) const -> const EPubOpf::SpineItem & {
        return opf_->getSpine(idx);
    }

    [[nodiscard]] auto findFileOffsetAtCharOffset(const std::string &path, uint32_t charOffset)
        -> uint32_t;

    [[nodiscard]] auto findCharOffsetAtFileOffset(const std::string &path, uint32_t fileOffset)
        -> uint32_t;

    [[nodiscard]] inline auto getSpineCount() const -> uint16_t { return opf_->getSpineCount(); }

    [[nodiscard]] auto getFile(const std::string &path)
        -> std::pair<std::shared_ptr<uint8_t[]>, uint32_t>;

    [[nodiscard]] inline auto getToc() const -> ReadableTableOfContentsChapterVector {
        return readable_->toc;
    }

    [[nodiscard]] inline auto getSpineIdx(const std::string &href) const -> uint16_t {
        return opf_->getSpineIdx(href);
    }

    [[nodiscard]] auto getXHTMLFile(const std::string &path) -> pugi::xml_document &;

    [[nodiscard]] inline auto getOffset(const pugi::char_t *item) const -> uint32_t {
        return item - (pugi::char_t *)currentFileBuffer_.get();
    }

    [[nodiscard]] inline auto getFullPath(const std::string &filename) const -> std::string {
        return opf_->getFullPath(filename);
    }

    [[nodiscard]] inline auto isOpen() const -> bool { return fileOpen_; }

    [[nodiscard]] auto getRelativeFilePath(const std::string &filename, Idx spineIdx) const
        -> std::string {
        std::string mainFilePath = opf_->getFullPath(opf_->getSpine(spineIdx).item->href);
        std::string path;
        opf_->extractPath(mainFilePath.c_str(), path);
        return path + filename;
    }

    [[nodiscard]] auto getUncompressedSize(Idx spineIdx) -> uint32_t {
        std::string filePath = opf_->getFullPath(opf_->getSpine(spineIdx).item->href);
        // log_w("Loading file %s", filePath.c_str());
        if (epubUnzipper_.openFile(filePath.c_str())) {
            uint32_t size = epubUnzipper_.getFileSize();
            epubUnzipper_.closeFile();
            return size;
        }
        return 0;
    }

    auto getReadableToCChapterIndex(const std::shared_ptr<EPubPlace> &ePubPlace) const -> int;

private:
    Unzipper epubUnzipper_;
    const std::shared_ptr<Readable> readable_;

    pugi::xml_document currentFileDoc_;

    bool fileOpen_{false};

    std::shared_ptr<EPubOpf> opf_{nullptr};

    // std::unique_ptr<pugi::char_t[]> currentFileBuffer_;
    std::shared_ptr<uint8_t[]> currentFileBuffer_;
    std::string currentFilePath_;

    auto open() -> bool;
    void close();
};
