#pragma once

#include <cstdint>

#include "Renderers/RendererDefs.hpp"

// Caches positions in the current EPub chapter to allow back anf forward page processing
// optimisation.

const int DISTANCE_BETWEEN_POSITIONS = 2000;
const int CACHE_SIZE = 500;
const int MAX_SPINE_COUNT = 30;

class EPubFile;

class EPubPositionCache {

    EPubFile &ePubFile_;

    uint16_t cacheTopIdx_{0};
    struct Cache {
        uint32_t chapterOffset;             // offsets from the beginning of chapter
        renderer_defs::Idx chapterFilesIdx; // Idx in the chapterFiles_ struct
    } cache_[CACHE_SIZE]{};

    uint16_t chapterFilesTopIdx_{0};
    struct ChapterFiles {
        uint32_t chapterOffset;
        renderer_defs::Idx spineIdx;
    } chapterFiles_[MAX_SPINE_COUNT]{};

    // Add a position in cache.
    void doAddPosition(uint32_t chapterOffset, const renderer_defs::Idx spineIdx);

    // Add a spine into chapterFiles_. The check on the space availability and the absence of the
    // spineIdx in the chapterFiles_ is already done.
    void doAddChapterFile(const renderer_defs::Idx spineIdx);

    auto computeChapterOffset(const renderer_defs::StreamPlace &streamPlace) -> int32_t;
    void checkChapterFilesCompleteness(const renderer_defs::Idx spineIdx);
    auto computeStreamPlace(renderer_defs::Idx cacheIdx) -> renderer_defs::StreamPlace;

    // Reset the caching content to be empty (only entry is the chapter first spine).
    void init(renderer_defs::Idx chapterSpineIdx);

public:
    EPubPositionCache(EPubFile &ePubFile) : ePubFile_(ePubFile) { init(0); };
    ~EPubPositionCache() = default;
    ;

    // This method must be called by the tokenizer, specifying a location in the current
    // chapter being processed, where a position could be added to the cache. It will then
    // verify if it is a good fit and if positive, add the  position into the cache.
    void addPosition(const renderer_defs::StreamPlace &streamPlace);

    // Get a position from the cache for page processing.
    auto getPosition(const renderer_defs::StreamPlace &fromPlace, bool backwards)
        -> renderer_defs::StreamPlace;
};