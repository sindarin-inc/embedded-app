#pragma once

#include <cstdint>

#include "EPubDefs.hpp"
#include "Renderers/RendererDefs.hpp"

// Caches positions in the current EPub chapter to allow back anf forward page processing
// optimisation.

const int DISTANCE_BETWEEN_POSITIONS = 2000;
const int PARA_CACHE_SIZE = 500;
const int PAGE_CACHE_SIZE = 500;
const int PAGE_DISTANCE = 5;
const int MAX_SPINE_COUNT = 30;

using CacheIdx = int32_t;
using Offset = uint32_t;
using PageNumber = int;
class EPubFile;

class EPubPositionCache {

    EPubFile &ePubFile_;

    renderer_defs::StreamPlace savedPagePosition_;

    CacheIdx paraCacheTopIdx_{0};
    struct ParaCache {
        Offset offset;            // offsets from the beginning of chapter
        CacheIdx chapterFilesIdx; // CacheIdx in the chapterFiles_ structure
    } paraCache_[PARA_CACHE_SIZE]{};

    PageNumber pageNumber_{0};
    CacheIdx pageCacheTopIdx_{0};
    struct PageCache {
        Offset offset;            // offsets from the beginning of chapter
        CacheIdx chapterFilesIdx; // CacheIdx in the chapterFiles_ structure
    } pageCache_[PAGE_CACHE_SIZE]{};

    CacheIdx chapterFilesTopIdx_{0};
    struct ChapterFiles {
        Offset offset;     // offsets from the beginning of chapter
        SpineIdx spineIdx; // SpineIdx in the OPF structure
    } chapterFiles_[MAX_SPINE_COUNT]{};

    // Add a paragraph position in cache.
    void doAddParagraphPosition(Offset offset, const SpineIdx spineIdx);

    // Add a page position in cache.
    void doAddPagePosition(Offset offset, const SpineIdx spineIdx);

    // Add a spine into chapterFiles_. The check on the space availability and the absence of the
    // spineIdx in the chapterFiles_ is already done.
    void doAddChapterFile(const SpineIdx spineIdx);

    auto computeChapterOffset(const renderer_defs::StreamPlace &streamPlace) -> Offset;
    void checkChapterFilesCompleteness(const SpineIdx spineIdx);
    auto computeParaStreamPlace(CacheIdx paraCacheIdx) -> renderer_defs::StreamPlace;
    auto computePageStreamPlace(CacheIdx pageCacheIdx) -> renderer_defs::StreamPlace;

    // Reset the caching content to be empty (only entry is the chapter first spine).
    void init(SpineIdx chapterSpineIdx);

public:
    EPubPositionCache(EPubFile &ePubFile) : ePubFile_(ePubFile) { init(0); };
    ~EPubPositionCache() = default;
    ;

    // This method must be called by the tokenizer through the EPubFile, specifying a location in
    // the current chapter being processed, where a paragraph position could be added to the cache.
    // It will then verify if it is a good fit and if positive, add the  position into the cache.
    void addParagraphPosition(const renderer_defs::StreamPlace &streamPlace);

    // Idem for this method, but for pages
    void addPagePosition(const renderer_defs::StreamPlace &streamPlace);

    // Get a paragraph position from the cache suitable to permit the EPub Tokenizer to set it's
    // internal state in line with the XHTML tags. The return location is pointing at a top-level
    // <p> or <div> tag. The computed page position is saved in the savedPagePosition variable.
    auto getParagraphPosition(const renderer_defs::StreamPlace &fromPlace, bool backwards)
        -> renderer_defs::StreamPlace;

    // Get a page position from the cache suitable to permit the EPub Tokenizer to start building at
    // a page boundary. The return location is pointing at a position that is the start of a page
    // located after the paragraphPosition parameter. The getParagraphPosition() method MUST have
    // been used as a previous call to retrieve a suitable top-level paragraph to permit this call
    // to work properly.
    [[nodiscard]] inline auto getPagePosition() -> renderer_defs::StreamPlace {
        return savedPagePosition_;
    }

    // Get positions (paragraph and page) from the cache for page processing.
    auto getPositions(const renderer_defs::StreamPlace &fromPlace, bool backwards)
        -> std::pair<renderer_defs::StreamPlace, renderer_defs::StreamPlace>;
};