#pragma once

#include "RendererDefs.hpp"

using namespace renderer_defs;

// Responsible of maintaining page indexes for the current EPub document being read.

class EPubPageMgr {

private:
    PageIndex pageIndex_{};

    Idx pageIndexHead_{0};
    Idx pageIndexTail_{0};

    Idx currentPageIndex_{0};
    uint16_t currentChapterNbr_{0};

    bool empty_{true};

public:
    EPubPageMgr() = default;

    void clear() {
        pageIndexHead_ = pageIndexTail_ = 0;
        currentPageIndex_ = 0;
        currentChapterNbr_ = 0;
        empty_ = true;
    }

    void incPageIndexTail() {
        pageIndexTail_ = (pageIndexTail_ + 1) % PAGE_INDEX_SIZE;
        if (pageIndexTail_ == pageIndexHead_) {
            pageIndexHead_ = (pageIndexHead_ + 1) % PAGE_INDEX_SIZE;
        }
    }

    void setFirstPageIndex(uint16_t chapterNbr, uint32_t offset) {
        clear();
        pageIndex_[pageIndexTail_] = offset;
        currentPageIndex_ = pageIndexTail_;
        currentChapterNbr_ = chapterNbr;
        incPageIndexTail();
        empty_ = false;
    }

    void addAndSetPageIndex(uint16_t chapterNbr, uint32_t offset) {
        if (!empty_ && (chapterNbr == currentChapterNbr_)) {
            pageIndex_[pageIndexTail_] = offset;
            currentPageIndex_ = pageIndexTail_;
            incPageIndexTail();
        } else {
            setFirstPageIndex(chapterNbr, offset);
        }
    }

    [[nodiscard]] auto getPreviousPageOffset(uint16_t chapterNbr) -> uint32_t {
        uint32_t offset = 0;
        if (!empty_ && (chapterNbr == currentChapterNbr_) &&
            (currentPageIndex_ != pageIndexHead_)) {
            currentPageIndex_ =
                (pageIndexHead_ == 0) ? (PAGE_INDEX_SIZE - 1) : (pageIndexHead_ - 1);
            offset = pageIndex_[currentPageIndex_];
        } else {
            // ToDo: Retrieve pages location before currentPageIndex_, or from the preceding chapter
            offset = pageIndex_[0];
        }
        return offset;
    }
};