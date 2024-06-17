#include "EPubPositionCache.hpp"

#include "EPubFile.hpp"

void EPubPositionCache::doAddParagraphPosition(Offset offset, const SpineIdx spineIdx) {
    for (CacheIdx chapterFilesIdx = chapterFilesTopIdx_; chapterFilesIdx >= 0; chapterFilesIdx--) {
        if (chapterFiles_[chapterFilesIdx].spineIdx == spineIdx) {
            paraCache_[++paraCacheTopIdx_] = {.offset = offset, .chapterFilesIdx = chapterFilesIdx};
            log_w("Adding new paragraph position, chapter files index: %d, chapter offset: %" PRIu32
                  ", "
                  "paragraph cache index %d",
                  chapterFilesIdx, offset, paraCacheTopIdx_);
            return;
        }
    }
    log_e("Spine not found in chapterFiles_ array! Spine Idx: %" PRIi32, spineIdx);
}

void EPubPositionCache::doAddPagePosition(Offset offset, const SpineIdx spineIdx) {
    for (CacheIdx chapterFilesIdx = chapterFilesTopIdx_; chapterFilesIdx >= 0; chapterFilesIdx--) {
        if (chapterFiles_[chapterFilesIdx].spineIdx == spineIdx) {
            pageCache_[++pageCacheTopIdx_] = {.offset = offset, .chapterFilesIdx = chapterFilesIdx};
            log_w("Adding new page position, chapter files index: %d, chapter offset: %" PRIu32
                  ", page "
                  "cache index %d",
                  chapterFilesIdx, offset, pageCacheTopIdx_);
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
        chapterFiles_[chapterFilesTopIdx_] = ChapterFiles{
            .offset = static_cast<Offset>(chapterFiles_[chapterFilesTopIdx_ - 1].offset +
                                          ePubFile_.getUncompressedSize(spineIdx - 1)),
            .spineIdx = spineIdx};
        log_w("Add Spine #%d at chapter offset %" PRIu32 ", at chapterFiles index %d", spineIdx,
              chapterFiles_[chapterFilesTopIdx_].offset, chapterFilesTopIdx_);
    } else {
        log_e("Too many chapterFiles in a chapter!");
    }
}

auto EPubPositionCache::computeChapterOffset(const renderer_defs::StreamPlace &streamPlace)
    -> Offset {
    for (CacheIdx idx = chapterFilesTopIdx_; idx >= 0; idx--) {
        if (chapterFiles_[idx].spineIdx == streamPlace.getSpineIdx()) {
            return chapterFiles_[idx].offset + streamPlace.getOffset();
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

auto EPubPositionCache::computeParaStreamPlace(CacheIdx paraCacheIdx)
    -> renderer_defs::StreamPlace {
    return {paraCache_[paraCacheIdx].offset -
                chapterFiles_[paraCache_[paraCacheIdx].chapterFilesIdx].offset,
            chapterFiles_[paraCache_[paraCacheIdx].chapterFilesIdx].spineIdx};
}

auto EPubPositionCache::computePageStreamPlace(CacheIdx pageCacheIdx)
    -> renderer_defs::StreamPlace {
    return {pageCache_[pageCacheIdx].offset -
                chapterFiles_[pageCache_[pageCacheIdx].chapterFilesIdx].offset,
            chapterFiles_[pageCache_[pageCacheIdx].chapterFilesIdx].spineIdx};
}

// Reset the caching content to be empty
void EPubPositionCache::init(SpineIdx chapterSpineIdx) {
    paraCacheTopIdx_ = pageCacheTopIdx_ = chapterFilesTopIdx_ = 0;
    paraCache_[0] = {.offset = 0, .chapterFilesIdx = 0};
    pageCache_[0] = {.offset = 0, .chapterFilesIdx = 0};
    pageNumber_ = 1;
    chapterFiles_[0] = {.offset = 0, .spineIdx = chapterSpineIdx};

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
        Offset diff = (offset - paraCache_[paraCacheTopIdx_].offset);
        // log_w("offset (%" PRIi32 ") - last cache offset (%" PRIu32 ") = %" PRIi32, offset,
        //       paraCache_[paraCacheTopIdx_].offset, diff);
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

    if ((pageNumber_++ % PAGE_DISTANCE) == 0) {
        // The usual case is when the place is in the first spine of a chapter. No need to do the
        // call to checkChapterFilesCompleteness() if it's the case.
        if (streamPlace.getSpineIdx() != chapterFiles_[0].spineIdx) {
            checkChapterFilesCompleteness(streamPlace.getSpineIdx());
        }

        if (pageCacheTopIdx_ < (PAGE_CACHE_SIZE - 1)) { // enough place available in the cache?
            Offset offset = computeChapterOffset(streamPlace);
            Offset diff = (offset - pageCache_[pageCacheTopIdx_].offset);
            // log_w("offset (%" PRIi32 ") - last cache offset (%" PRIu32 ") = %" PRIi32, offset,
            //       pageCache_[pageCacheTopIdx_].offset, diff);
            if (diff >= 0) {
                doAddPagePosition(offset, streamPlace.getSpineIdx());
            }
        }
    }
}

auto EPubPositionCache::getParagraphPosition(const renderer_defs::StreamPlace &fromPlace,
                                             bool backwards) -> renderer_defs::StreamPlace {

    log_w("Search for location %s, %s", fromPlace.str().c_str(),
          backwards ? "BACKWARD" : "FORWARD");

    if (fromPlace.getSpineIdx() != chapterFiles_[0].spineIdx) {
        checkChapterFilesCompleteness(fromPlace.getSpineIdx());
    }

    CacheIdx paraCacheIdx;

    if (paraCacheTopIdx_ == 0) { // Only one entry in the paragraphs' cache
        paraCacheIdx = 0;
    } else {
        // Find the location in the cache that is just before the fromPlace location

        Offset offset = computeChapterOffset(fromPlace);
        for (paraCacheIdx = paraCacheTopIdx_; paraCacheIdx > 0; paraCacheIdx--) {
            if (paraCache_[paraCacheIdx].offset < offset) {
                break;
            }
        }

        if (backwards) {
            log_w("Going backward. paraCacheIdx : %d", paraCacheIdx);

            if (paraCacheIdx == 0) {
                if (chapterFiles_[0].spineIdx == 0) {
                    std::cout << "---- A ----" << std::endl;
                    return {0, 0};
                } else {
                    SpineIdx chapterSpineIdx =
                        ePubFile_.getChapterFirstSpineIdx(chapterFiles_[0].spineIdx - 1);
                    if (chapterSpineIdx < 0) {
                        std::cout << "---- B ----" << std::endl;
                        return {0, 0};
                    } else {
                        std::cout << "---- C ----" << std::endl;
                        return {0, chapterFiles_[paraCache_[paraCacheIdx].chapterFilesIdx]
                                       .spineIdx /* chapterSpineIdx */};
                    }
                }
            }
        }
    }

    // Now, as there is no garanty that the beginning of a page in the cache would be at least at a
    // DISTANCE_BETWEEN_POSITIONS from fromPlace, we go back until a page start was found.

    paraCacheIdx = (paraCacheIdx > 0) ? paraCacheIdx - 1 : 0;

    CacheIdx pageCacheIdx;
    for (pageCacheIdx = pageCacheTopIdx_; pageCacheIdx > 0; pageCacheIdx--) {
        if (pageCache_[pageCacheIdx].offset <= paraCache_[paraCacheIdx].offset) {
            break;
        }
    }

    for (paraCacheIdx = paraCacheIdx; paraCacheIdx > 0; paraCacheIdx--) {
        if (paraCache_[paraCacheIdx].offset <= pageCache_[pageCacheIdx].offset) {
            break;
        }
    }

    renderer_defs::StreamPlace paraStreamPlace =
        (paraCacheIdx == 0) ? renderer_defs::StreamPlace(0, chapterFiles_[0].spineIdx)
                            : computeParaStreamPlace(paraCacheIdx - 1);

    log_w("Returned paragraph position: %s", paraStreamPlace.str().c_str());

    return paraStreamPlace;
}

auto EPubPositionCache::getPositions(const renderer_defs::StreamPlace &fromPlace, bool backwards)
    -> std::pair<renderer_defs::StreamPlace, renderer_defs::StreamPlace> {

    renderer_defs::StreamPlace paraStreamPlace = getParagraphPosition(fromPlace, backwards);

    log_w("Returned PARAGRAPH position: %s, PAGE position: %s", paraStreamPlace.str().c_str(),
          savedPagePosition_.str().c_str());

    return {paraStreamPlace, savedPagePosition_};
}
