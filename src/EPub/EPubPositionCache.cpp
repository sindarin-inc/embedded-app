#include "EPubPositionCache.hpp"

#include "EPubFile.hpp"

void EPubPositionCache::doAddParagraphPosition(Offset chapterOffset, const SpineIdx spineIdx) {
    for (CacheIdx chapterFilesIdx = chapterFilesTopIdx_; chapterFilesIdx >= 0; chapterFilesIdx--) {
        if (chapterFiles_[chapterFilesIdx].spineIdx == spineIdx) {
            paraCache_[++paraCacheTopIdx_] = {.chapterOffset = chapterOffset,
                                              .chapterFilesIdx = chapterFilesIdx};
            log_w("Adding new paragraph position, chapter files index: %d, chapter offset: %d, "
                  "paragraph cache index %d",
                  chapterFilesIdx, chapterOffset, paraCacheTopIdx_);
            return;
        }
    }
    log_e("Spine not found in chapterFiles_ array! Spine Idx: %" PRIi32, spineIdx);
}

void EPubPositionCache::doAddPagePosition(Offset chapterOffset, const SpineIdx spineIdx) {
    for (CacheIdx chapterFilesIdx = chapterFilesTopIdx_; chapterFilesIdx >= 0; chapterFilesIdx--) {
        if (chapterFiles_[chapterFilesIdx].spineIdx == spineIdx) {
            pageCache_[++pageCacheTopIdx_] = {.chapterOffset = chapterOffset,
                                              .chapterFilesIdx = chapterFilesIdx};
            log_w("Adding new page position, chapter files index: %d, chapter offset: %d, page "
                  "cache index %d",
                  chapterFilesIdx, chapterOffset, pageCacheTopIdx_);
            return;
        }
    }
    log_e("Spine not found in chapterFiles_ array! Spine Idx: %d", spineIdx);
}

// Add a spine into chapterFiles_. The check on the space availability and the absence of the
// spineIdx in the chapterFiles_ is already done.
void EPubPositionCache::doAddChapterFile(const SpineIdx spineIdx) {
    if (chapterFilesTopIdx_ < MAX_SPINE_COUNT) {
        chapterFilesTopIdx_++;
        chapterFiles_[chapterFilesTopIdx_] =
            ChapterFiles{.chapterOffset = chapterFiles_[chapterFilesTopIdx_ - 1].chapterOffset +
                                          ePubFile_.getUncompressedSize(spineIdx - 1),
                         .spineIdx = spineIdx};
        log_w("Add Spine #%d at chapter offset %d, at chapterFiles index %d", spineIdx,
              chapterFiles_[chapterFilesTopIdx_].chapterOffset, chapterFilesTopIdx_);
    } else {
        log_e("Too many chapterFiles in a chapter!");
    }
}

auto EPubPositionCache::computeChapterOffset(const renderer_defs::StreamPlace &streamPlace)
    -> Offset {
    for (CacheIdx idx = chapterFilesTopIdx_; idx >= 0; idx--) {
        if (chapterFiles_[idx].spineIdx == streamPlace.getSpineIdx()) {
            return chapterFiles_[idx].chapterOffset + streamPlace.getOffset();
        }
    }
    log_e("Spine not found in chapterFiles_ array! Spine Idx: %d", streamPlace.getSpineIdx());

    return 0;
}

void EPubPositionCache::checkChapterFilesCompleteness(const SpineIdx spineIdx) {
    // If the current chapter is not the one in the cache, we reinitialize the cache
    // and update the chapterFiles_ array with all the chapter files that are part of the chapter,
    // up to the one received in &place
    SpineIdx chapterSpineIdx = ePubFile_.getChapterFirstSpineIdx(spineIdx);

    if (ePubFile_.spineIdxIsChapterStart(spineIdx) && (chapterSpineIdx != spineIdx)) {
        log_e("OPF discripancies!!! #%d vs #%d", chapterSpineIdx, spineIdx);
    }
    if (chapterSpineIdx != chapterFiles_[0].spineIdx) {
        log_w("Resetting cache, change of chapter start spine to %d", chapterSpineIdx);
        // There was a change of chapter. Initialize everything
        init(chapterSpineIdx);
        // The following is required in case when the first spine in a chapter is not
        // containing any first-level paragraph.
        for (SpineIdx idx = chapterSpineIdx + 1; idx <= spineIdx; idx++) {
            doAddChapterFile(idx);
        }
    } else {
        // The following is required in case when some spine in a chapter is not
        // containing any first-level paragraph.
        for (SpineIdx idx = chapterFiles_[chapterFilesTopIdx_].spineIdx + 1; idx <= spineIdx;
             idx++) {
            doAddChapterFile(idx);
        }
    }
}

auto EPubPositionCache::computeStreamPlace(CacheIdx cacheIdx) -> renderer_defs::StreamPlace {
    return renderer_defs::StreamPlace(
        paraCache_[cacheIdx].chapterOffset -
            chapterFiles_[paraCache_[cacheIdx].chapterFilesIdx].chapterOffset,
        chapterFiles_[paraCache_[cacheIdx].chapterFilesIdx].spineIdx);
}

// Reset the caching content to be empty
void EPubPositionCache::init(SpineIdx chapterSpineIdx) {
    paraCacheTopIdx_ = pageCacheTopIdx_ = chapterFilesTopIdx_ = 0;
    paraCache_[0] = {.chapterOffset = 0, .chapterFilesIdx = 0};
    pageCache_[0] = {.chapterOffset = 0, .chapterFilesIdx = 0};
    pageNumber = 1;
    chapterFiles_[0] = {.chapterOffset = 0, .spineIdx = chapterSpineIdx};

    log_w("Cache initialized with chapter spine #%" PRIi16, chapterSpineIdx);
}

// This method must be called by the tokenizer, specifying a location in the current
// chapter being processed, where a position could be added to the cache. It will then
// verify if it is a good fit and if positive, add the  position into the cache.
void EPubPositionCache::addParagraphPosition(const renderer_defs::StreamPlace &streamPlace) {
    // The usual case is when the place is in the first spine of a chapter. No need to do the
    // call to checkChapterFilesCompleteness() if it's the case.
    if (streamPlace.getSpineIdx() != chapterFiles_[0].spineIdx) {
        checkChapterFilesCompleteness(streamPlace.getSpineIdx());
    }

    if (paraCacheTopIdx_ < (PARA_CACHE_SIZE - 1)) { // enough place available in the cache?
        Offset offset = computeChapterOffset(streamPlace);
        Offset diff = (offset - paraCache_[paraCacheTopIdx_].chapterOffset);
        // log_d("offset (%" PRIi32 ") - last cache offset (%" PRIu32 ") = %" PRIi32, offset,
        //       paraCache_[paraCacheTopIdx_].chapterOffset, diff);
        if (diff >= DISTANCE_BETWEEN_POSITIONS) {
            doAddParagraphPosition(offset, streamPlace.getSpineIdx());
        }
    }
}

// This method must be called by the tokenizer, specifying a location in the current
// chapter being processed, where a page position could be added to the cache. It will then
// verify if it is a good fit (every PAGE_DISTANCE and at the end of the list) and if positive, add
// the  position into the cache.
void EPubPositionCache::addPagePosition(const renderer_defs::StreamPlace &streamPlace) {

    if ((pageNumber++ % PAGE_DISTANCE) == 0) {
        // The usual case is when the place is in the first spine of a chapter. No need to do the
        // call to checkChapterFilesCompleteness() if it's the case.
        if (streamPlace.getSpineIdx() != chapterFiles_[0].spineIdx) {
            checkChapterFilesCompleteness(streamPlace.getSpineIdx());
        }

        if (pageCacheTopIdx_ < (PAGE_CACHE_SIZE - 1)) { // enough place available in the cache?
            Offset offset = computeChapterOffset(streamPlace);
            Offset diff = (offset - pageCache_[pageCacheTopIdx_].chapterOffset);
            // log_d("offset (%" PRIi32 ") - last cache offset (%" PRIu32 ") = %" PRIi32, offset,
            //       pageCache_[pageCacheTopIdx_].chapterOffset, diff);
            if (diff >= 0) {
                doAddPagePosition(offset, streamPlace.getSpineIdx());
            }
        }
    }
}

// Get a position from the cache for page processing.
auto EPubPositionCache::getPositions(const renderer_defs::StreamPlace &fromPlace, bool backwards)
    -> std::pair<renderer_defs::StreamPlace, renderer_defs::StreamPlace> {

    log_d("Search for location %s, %s", fromPlace.str(), backwards ? "BACKWARD" : "FORWARD");

    if (fromPlace.getSpineIdx() != chapterFiles_[0].spineIdx) {
        checkChapterFilesCompleteness(fromPlace.getSpineIdx());
    }

    renderer_defs::StreamPlace paraStreamPlace;
    renderer_defs::StreamPlace pageStreamPlace;

    if ((paraCacheTopIdx_ == 0) && (pageCacheTopIdx_ == 0)) {
        paraStreamPlace = pageStreamPlace =
            renderer_defs::StreamPlace(0, chapterFiles_[0].spineIdx);
    } else {
        // Find the location in the cache that is just before the fromPlace location

        Offset offset = computeChapterOffset(fromPlace);
        CacheIdx cacheIdx;
        for (cacheIdx = paraCacheTopIdx_; cacheIdx > 0; cacheIdx--) {
            if (paraCache_[cacheIdx].chapterOffset < offset) {
                break;
            }
        }

        if (backwards) {
            log_w("Going backward. cacheIdx : %d", cacheIdx);

            if (cacheIdx == 0) {
                if (chapterFiles_[0].spineIdx == 0) {
                    std::cout << "---- A ----" << std::endl;
                    return renderer_defs::StreamPlace(0, 0);
                } else {
                    SpineIdx chapterSpineIdx =
                        ePubFile_.getChapterFirstSpineIdx(chapterFiles_[0].spineIdx - 1);
                    if (chapterSpineIdx < 0) {
                        std::cout << "---- B ----" << std::endl;
                        return renderer_defs::StreamPlace(0, 0);
                    } else {
                        std::cout << "---- C ----" << std::endl;
                        return renderer_defs::StreamPlace(
                            0, chapterFiles_[paraCache_[cacheIdx].chapterFilesIdx]
                                   .spineIdx /* chapterSpineIdx */);
                    }
                }
            } else {
                std::cout << "---- D ----" << std::endl;
                return computeStreamPlace(cacheIdx - 1);
            }
        } else {
            paraStreamPlace = (cacheIdx == 0)
                                  ? renderer_defs::StreamPlace(0, chapterFiles_[0].spineIdx)
                                  : computeStreamPlace(cacheIdx - 1);
        }
    }

    log_d("Returned paragraph position: %s, page position: %s", paraStreamPlace.str(),
          pageStreamPlace.str());

    return {paraStreamPlace, pageStreamPlace};
}