#pragma once

#include <pugixml.hpp>
#include <utility>

#include "EPubDefs.hpp"
#include "EPubOpf.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "Misc/Unzipper.hpp"
#include "Models/DocType.hpp"
#include "Models/Readable.hpp"
#include "Models/UserReadablePartialChapterSummary.hpp"
#include "Renderers/RendererStream.hpp"

class EPubFile : public RendererStream {
public:
    EPubFile(const SFile &file, std::shared_ptr<Readable> readable)
        : RendererStream(DocType::EPUB), epubUnzipper_(file), readable_(std::move(readable)),
          currentFileBuffer_(nullptr) {
        open();
    }

    ~EPubFile() override { close(); };

    [[nodiscard]] auto read() -> int override { return 0; }
    [[nodiscard]] auto seek(uint32_t pos) -> bool override { return false; }
    [[nodiscard]] auto position() -> size_t override { return 0; }
    [[nodiscard]] auto size() -> size_t override { return 0; /* opf_->getTotalSize(); */ }

    [[nodiscard]] auto isAnArticle() const -> bool { return readable_->isArticle(); }

    // [[nodiscard]] inline auto getSpineIdxForOffset(uint32_t offset) const -> SpineIdx {
    //     return opf_->getSpineIdxForOffset(offset);
    // }

    [[nodiscard]] inline auto getSpine(SpineIdx idx) const -> const EPubOpf::SpineItem & {
        return opf_->getSpine(idx);
    }

    [[nodiscard]] auto findFileOffsetAtCharOffset(const std::string &path, uint32_t charOffset)
        -> uint32_t;

    [[nodiscard]] auto findCharOffsetAtFileOffset(const std::string_view &path, uint32_t fileOffset)
        -> uint32_t;

    [[nodiscard]] inline auto getSpineCount() const -> uint16_t { return opf_->getSpineCount(); }

    [[nodiscard]] auto getFile(const std::string &path)
        -> std::pair<std::shared_ptr<uint8_t[]>, uint32_t>;

    [[nodiscard]] inline auto getSpineIdx(const std::string_view &href) const -> SpineIdx {
        return opf_->getSpineIdx(href);
    }

    [[nodiscard]] inline auto getHrefByManifestId(const std::string &manifestId) const
        -> std::string_view {
        return opf_->getHrefByManifestId(manifestId);
    }

    [[nodiscard]] inline auto getManifestIdByHref(const std::string &href) const -> std::string {
        return opf_->getManifestIdByHref(href);
    }

    [[nodiscard]] auto getXHTMLFile(const std::string_view &path) -> pugi::xml_document &;

    [[nodiscard]] inline auto getOffset(const pugi::char_t *item) const -> uint32_t {
        return item - reinterpret_cast<pugi::char_t *>(currentFileBuffer_.get());
    }

    [[nodiscard]] inline auto getFullPath(const std::string &filename) const -> std::string {
        return opf_->getFullPath(filename);
    }

    [[nodiscard]] inline auto isOpen() const -> bool { return fileOpen_; }

    [[nodiscard]] auto getRelativeFilePath(const std::string &filename, SpineIdx spineIdx) const
        -> std::string {
        std::string mainFilePath =
            opf_->getFullPath(static_cast<std::string>(opf_->getSpine(spineIdx).item->href));
        std::string path;
        opf_->extractPath(mainFilePath.c_str(), path);
        return path + filename;
    }

    auto getUncompressedSize(const std::string &href) -> uint32_t;
    auto getUncompressedSize(SpineIdx spineIdx) -> uint32_t;

    auto getChapterIdx(const std::shared_ptr<EPubPlace> &ePubPlace) const -> ChapterIdx;
    auto getChapterIdx(const std::string_view &href) const -> ChapterIdx;
    auto getChapterIdx(SpineIdx spineIdx) const -> ChapterIdx;
    auto spineIdxIsChapterStart(SpineIdx spineIdx) const -> bool;
    auto getChapterFirstSpineIdx(SpineIdx spineIdx) const -> SpineIdx;
    auto getOffsetPreviousSpineItemsInChapter(SpineIdx spineIdx) -> uint32_t;

    auto hasToc() -> bool;
    auto getToc() -> ReadableTableOfContentsChapterVector;

    auto hasEpubAddition(const std::string_view &spineHref, const std::string &additionType)
        -> bool;
    auto hasSummary(const std::string_view &spineHref) -> bool;
    auto getSummaryContent(
        const std::shared_ptr<EPubPlace> &placeInReadable,
        std::shared_ptr<TxtPlace> &placeInSummary,
        const std::shared_ptr<UserReadablePartialChapterSummary> &partialChapterSummary,
        bool includeUnread) -> SpiramString;
    auto hasDictionary(const std::string_view &spineHref) -> bool;
    auto getDictionaryContent(const std::shared_ptr<EPubPlace> &placeInReadable,
                              std::shared_ptr<TxtPlace> &placeInDictionary) -> SpiramString;

    void close();

private:
    Unzipper epubUnzipper_;
    const std::shared_ptr<Readable> readable_;
    std::shared_ptr<ReadableTableOfContentsChapterVector> toc_;

    pugi::xml_document currentFileDoc_;

    bool fileOpen_{false};

    std::shared_ptr<EPubOpf> opf_{nullptr};

    // std::unique_ptr<pugi::char_t[]> currentFileBuffer_;
    std::shared_ptr<uint8_t[]> currentFileBuffer_;
    std::string currentFilePath_;

    auto open() -> bool;
};
