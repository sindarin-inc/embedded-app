#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "EPubDefs.hpp"
#include "Models/Readable.hpp"

class EPubOpf {
public:
    EPubOpf(const char *path) : path_(path) {}

    struct ManifestItem {
        SpiramString id;
        SpiramString href;
        SpiramString mediaType;
    };
    struct SpineItem {
        ManifestItem *item;
        int chapterIdx;
        bool chapterStart;
    };

    void parse(const std::shared_ptr<uint8_t[]> &buffer, uint32_t length,
               const std::shared_ptr<Readable> &readable);
    void loadSpine(const ReadableTableOfContentsChapterVector &toc);
    auto findChapterIdxInToc(const ReadableTableOfContentsChapterVector &toc, SpineItem &spineItem)
        -> ChapterIdx;

    [[nodiscard]] auto getSpine(SpineIdx idx) const -> const SpineItem & { return spine_[idx]; }

    [[nodiscard]] auto getFullPath(const std::string &fileName) const -> std::string {
        return basePath_ + fileName;
    }

    [[nodiscard]] inline auto getSpineCount() const -> uint16_t { return spine_.size(); }

    void extractPath(const char *fname, std::string &path);

    auto getSpineIdx(const std::string &href) const -> SpineIdx;

    auto getChapterIdx(const std::string &href) const -> ChapterIdx;
    auto getChapterIdx(SpineIdx spineIdx) const -> ChapterIdx;
    auto spineIdxIsChapterStart(SpineIdx spineIdx) const -> bool;
    auto getChapterFirstSpineIdx(SpineIdx spineIdx) const -> SpineIdx;
    auto getChapterFirstHref(const std::string &spineHRef) const -> std::string;

    auto exists(const std::string &id) const -> bool;

    // TODO: It'd probably be good practice to use a type alias for manifest IDs
    auto getHrefByManifestId(const std::string &manifestId) const -> std::string;
    auto getManifestIdByHref(const std::string &href) const -> std::string;

private:
    const char *path_;

    std::string basePath_;

    using ManifestMap = SpiramMap<SpiramString, ManifestItem>;
    using ManifestIterator = SpiramMap<SpiramString, ManifestItem>::iterator;
    using SpineVector = SpiramVector<SpineItem>;

    std::string title_;
    std::string creator_;
    ManifestMap manifest_;
    SpineVector spine_;
};
