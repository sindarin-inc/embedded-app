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

using CacheIdx = int;
using Offset = int;
using PageNumber = int;
class EPubFile;

class EPubPositionCache {

    EPubFile &ePubFile_;

    CacheIdx paraCacheTopIdx_{-1};
    struct ParaCache {
        Offset chapterOffset;     // offsets from the beginning of chapter
        CacheIdx chapterFilesIdx; // CacheIdx in the chapterFiles_ struct
    } paraCache_[PARA_CACHE_SIZE];

    PageNumber pageNumber{0};
    CacheIdx pageCacheTopIdx_{-1};
    struct PageCache {
        Offset chapterOffset;
        CacheIdx chapterFilesIdx;
    } pageCache_[PAGE_CACHE_SIZE];

    CacheIdx chapterFilesTopIdx_{-1};
    struct ChapterFiles {
        Offset chapterOffset;
        SpineIdx spineIdx;
    } chapterFiles_[MAX_SPINE_COUNT];

    // Add a paragraph position in cache.
    void doAddParagraphPosition(Offset chapterOffset, const SpineIdx spineIdx);

    // Add a page position in cache.
    void doAddPagePosition(Offset chapterOffset, const SpineIdx spineIdx);

    // Add a spine into chapterFiles_. The check on the space availability and the absence of the
    // spineIdx in the chapterFiles_ is already done.
    void doAddChapterFile(const SpineIdx spineIdx);

    auto computeChapterOffset(const renderer_defs::StreamPlace &streamPlace) -> Offset;
    void checkChapterFilesCompleteness(const SpineIdx spineIdx);
    auto computeStreamPlace(CacheIdx cacheIdx) -> renderer_defs::StreamPlace;

    // Reset the caching content to be empty (only entry is the chapter first spine).
    void init(SpineIdx chapterSpineIdx);

public:
    EPubPositionCache(EPubFile &ePubFile) : ePubFile_(ePubFile) { init(0); };
    ~EPubPositionCache(){};

    // This method must be called by the tokenizer through the EPubFile, specifying a location in
    // the current chapter being processed, where a paragraph position could be added to the cache.
    // It will then verify if it is a good fit and if positive, add the  position into the cache.
    void addParagraphPosition(const renderer_defs::StreamPlace &streamPlace);

    // Idem for this method, but for pages
    void addPagePosition(const renderer_defs::StreamPlace &streamPlace);

    // Get a position from the cache for page processing.
    auto getPositions(const renderer_defs::StreamPlace &fromPlace, bool backwards)
        -> std::pair<renderer_defs::StreamPlace, renderer_defs::StreamPlace>;
};