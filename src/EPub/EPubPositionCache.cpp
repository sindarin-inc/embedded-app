#include "EPubPositionCache.hpp"

#include "EPubFile.hpp"

void EPubPositionCache::doAddPosition(uint32_t chapterOffset, const renderer_defs::Idx spineIdx) {
    for (renderer_defs::Idx chapterFilesIdx = chapterFilesTopIdx_; chapterFilesIdx >= 0;
         chapterFilesIdx--) {
        if (chapterFiles_[chapterFilesIdx].spineIdx == spineIdx) {
            cache_[++cacheTopIdx_] = {.chapterOffset = chapterOffset,
                                      .chapterFilesIdx = chapterFilesIdx};
            log_w("Adding New position, chapter files index: %" PRIi32 ", chapter offset: %" PRIu32
                  ", cache index %" PRIi16,
                  chapterFilesIdx, chapterOffset, cacheTopIdx_);
            return;
        }
    }
    log_e("Spine not found in chapterFiles_ array! Spine Idx: %" PRIi32, spineIdx);
}

// Add a spine into chapterFiles_. The check on the space availability and the absence of the
// spineIdx in the chapterFiles_ is already done.
void EPubPositionCache::doAddChapterFile(const renderer_defs::Idx spineIdx) {
    if (chapterFilesTopIdx_ < MAX_SPINE_COUNT) {
        chapterFilesTopIdx_++;
        chapterFiles_[chapterFilesTopIdx_] =
            ChapterFiles{.chapterOffset = chapterFiles_[chapterFilesTopIdx_ - 1].chapterOffset +
                                          ePubFile_.getUncompressedSize(spineIdx - 1),
                         .spineIdx = spineIdx};
        log_w("Add Spine %" PRIi32 " at chapter offset %" PRIu32 ", at chapterFiles index %" PRIi16,
              spineIdx, chapterFiles_[chapterFilesTopIdx_].chapterOffset, chapterFilesTopIdx_);
    } else {
        log_e("Too many chapterFiles in a chapter!");
    }
}

auto EPubPositionCache::computeChapterOffset(const renderer_defs::StreamPlace &streamPlace)
    -> int32_t {
    for (renderer_defs::Idx idx = chapterFilesTopIdx_; idx >= 0; idx--) {
        if (chapterFiles_[idx].spineIdx == streamPlace.getSpineIdx()) {
            return chapterFiles_[idx].chapterOffset + streamPlace.getOffset();
        }
    }
    log_e("Spine not found in chapterFiles_ array! Spine Idx: %" PRIi32, streamPlace.getSpineIdx());

    return 0;
}

void EPubPositionCache::checkChapterFilesCompleteness(const renderer_defs::Idx spineIdx) {
    // If the current chapter is not the one in the cache, we reinitialize the cache
    // and update the chapterFiles_ array with all the chapter files that are part of the chapter,
    // up to the one received in &place
    renderer_defs::Idx chapterSpineIdx = ePubFile_.getChapterIdx(spineIdx);
    if (chapterSpineIdx != chapterFiles_[0].spineIdx) {
        // There was a change of chapter. Initialize everything
        init(chapterSpineIdx);
        // The following is required in case when the first spine in a chapter is not
        // containing any first-level paragraph.
        for (renderer_defs::Idx idx = chapterSpineIdx + 1; idx <= spineIdx; idx++) {
            doAddChapterFile(idx);
        }
    } else {
        // The following is required in case when some spine in a chapter is not
        // containing any first-level paragraph.
        for (renderer_defs::Idx idx = chapterFiles_[chapterFilesTopIdx_].spineIdx + 1;
             idx <= spineIdx; idx++) {
            doAddChapterFile(idx);
        }
    }
}

auto EPubPositionCache::computeStreamPlace(renderer_defs::Idx cacheIdx)
    -> renderer_defs::StreamPlace {
    return renderer_defs::StreamPlace(
        cache_[cacheIdx].chapterOffset -
            chapterFiles_[cache_[cacheIdx].chapterFilesIdx].chapterOffset,
        chapterFiles_[cache_[cacheIdx].chapterFilesIdx].spineIdx);
}

// Reset the caching content to be empty
void EPubPositionCache::init(renderer_defs::Idx chapterSpineIdx) {
    cacheTopIdx_ = chapterFilesTopIdx_ = 0;
    cache_[0] = {.chapterOffset = 0, .chapterFilesIdx = 0};
    chapterFiles_[0] = {.chapterOffset = 0, .spineIdx = chapterSpineIdx};
    log_w("Cache initialized with chapter spine %" PRIi16, chapterSpineIdx);
}

// This method must be called by the tokenizer, specifying a location in the current
// chapter being processed, where a position could be added to the cache. It will then
// verify if it is a good fit and if positive, add the  position into the cache.
void EPubPositionCache::addPosition(const renderer_defs::StreamPlace &streamPlace) {
    // The usual case is when the place is in the first spine of a chapter. No need to do the
    // call to checkChapterFilesCompleteness() if it's the case.
    if (streamPlace.getSpineIdx() != chapterFiles_[0].spineIdx) {
        checkChapterFilesCompleteness(streamPlace.getSpineIdx());
    }

    if (cacheTopIdx_ < (CACHE_SIZE - 1)) { // enough place available in the cache?
        int32_t offset = computeChapterOffset(streamPlace);
        int32_t diff = (offset - cache_[cacheTopIdx_].chapterOffset);
        log_d("offset (%" PRIi32 ") - last cache offset (%" PRIu32 ") = %" PRIi32, offset,
              cache_[cacheTopIdx_].chapterOffset, diff);
        if (diff >= DISTANCE_BETWEEN_POSITIONS) {
            doAddPosition(offset, streamPlace.getSpineIdx());
        }
    }
}

// Get a position from the cache for page processing.
auto EPubPositionCache::getPosition(const renderer_defs::StreamPlace &fromPlace,
                                    bool backwards) -> renderer_defs::StreamPlace {

    log_d("Search for offset %" PRIu32 " and spineIdx %" PRIi16 " %s", fromPlace.getOffset(),
          fromPlace.getSpineIdx(), backwards ? "BACKWARDS" : "FORWARD");

    if (fromPlace.getSpineIdx() != chapterFiles_[0].spineIdx) {
        checkChapterFilesCompleteness(fromPlace.getSpineIdx());
    }

    renderer_defs::StreamPlace streamPlace;

    if (cacheTopIdx_ == 0) {
        streamPlace = renderer_defs::StreamPlace(0, chapterFiles_[0].spineIdx);
    } else {
        // Find the location in the cache that is just before the fromPlace location

        int32_t offset = computeChapterOffset(fromPlace);
        uint16_t cacheIdx;
        for (cacheIdx = cacheTopIdx_; cacheIdx > 0; cacheIdx--) {
            if (cache_[cacheIdx].chapterOffset < offset) {
                break;
            }
        }

        if (backwards) {
            log_w("Going backward. cacheIdx : %" PRIi16, cacheIdx);

            if (cacheIdx == 0) {
                if (chapterFiles_[0].spineIdx == 0) {
                    return renderer_defs::StreamPlace(0, 0);
                } else {
                    renderer_defs::Idx chapterSpineIdx =
                        ePubFile_.getChapterIdx(chapterFiles_[0].spineIdx - 1);
                    if (chapterSpineIdx < 0) {
                        return renderer_defs::StreamPlace(0, 0);
                    } else {
                        return renderer_defs::StreamPlace(0, chapterSpineIdx);
                    }
                }
            } else {
                return computeStreamPlace(cacheIdx - 1);
            }
        } else {
            streamPlace = (cacheIdx == 0) ? renderer_defs::StreamPlace(0, chapterFiles_[0].spineIdx)
                                          : computeStreamPlace(cacheIdx - 1);
        }
    }

    log_d("Returned cached position: offset: %" PRIu32 ", spine %" PRIi16, streamPlace.getOffset(),
          streamPlace.getSpineIdx());

    return streamPlace;
}