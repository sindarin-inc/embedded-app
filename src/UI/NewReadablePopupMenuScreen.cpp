#include "UI/NewReadablePopupMenuScreen.hpp"

#include "UI/MsgPopup.hpp"

void NewReadablePopupMenuScreen::setUpPopupMenu() {
    std::shared_ptr<Place> place = renderer_.getCurrentPlace();

    std::string placeStringForBottom;
    std::string chapterPagesRemainingString;

    if (place->isFor(DocType::EPUB)) {
        auto stream = std::dynamic_pointer_cast<EPubFile>(renderer_.getStream());
        auto ePubPlace = std::dynamic_pointer_cast<EPubPlace>(place);
        auto &chapterHref = ePubPlace->getHRef();
        currentChapterIdx_ = stream->getChapterIdx(chapterHref);

        if (currentChapterIdx_ >= 0) {
            const auto &toc = stream->getToc();
            auto &chapter = toc[currentChapterIdx_];
            auto spineIdx = renderer_.getStreamPlace(renderer_.getCurrentPlace()).getSpineIdx();
            // TODO(john): This isn't exactly right because `place->getOffset()` is a byte offset,
            // not a letter offset from the beginning of the chapter. The chapters always start with
            // extra html-y stuff at the start so this is always inflated by some amount.

            // If current spine item is not the first in the chapter, add offsets of previous spine
            // items in chapter to get the correct offset
            auto previousSpineOffset = stream->getOffsetPreviousSpineItemsInChapter(spineIdx);
            auto currentOffset =
                chapter.epubOffset + previousSpineOffset + renderer_.getCurrentPlace()->getOffset();

            // Get the end offset of the last chapter (to determine the length of the book)
            auto &lastChapter = toc[stream->getToc().size() - 1];
            auto lastChapterEndOffset = lastChapter.endEpubOffset;

            if (currentOffset < lastChapterEndOffset) {
                if (stream->isAnArticle()) {
                    placeStringForBottom =
                        StringFormat("%d of %d page%s", SOL_PAGE_NUMBER(currentOffset),
                                     SOL_PAGE_NUMBER(lastChapterEndOffset),
                                     SOL_PAGE_NUMBER(lastChapterEndOffset) == 1 ? "" : "s");
                } else {
                    placeStringForBottom = StringFormat("%d of %d", SOL_PAGE_NUMBER(currentOffset),
                                                        SOL_PAGE_NUMBER(lastChapterEndOffset));
                }
            }

            // Determine how many Sol Pages are left in the chapter
            if (chapter.endEpubOffset > currentOffset && !stream->isAnArticle()) {
                auto chapterPagesRemaining = chapter.endEpubOffset - currentOffset;
                chapterPagesRemainingString =
                    StringFormat("%d page%s left", SOL_PAGE_NUMBER(chapterPagesRemaining),
                                 SOL_PAGE_NUMBER(chapterPagesRemaining) == 1 ? "" : "s");
            }
            // log_w("Chapter %d, %s, current:%d=%" PRIu32 "+%" PRIu32 "(%" PRIu32 ") end: %"
            // PRIu32,
            //       currentChapterIdx_, chapter.title.c_str(), currentOffset, chapter.epubOffset,
            //       renderer_.getTxtOffset(), renderer_.getCurrentPlace()->getOffset(),
            //       lastChapterEndOffset);
        } else {
            log_w("Couldn't find chapter index (%d) for href %s", currentChapterIdx_,
                  chapterHref.c_str());
        }
    } else {
        size_t lengthOfFile = renderer_.size();
        placeStringForBottom = StringFormat("%d of %d", SOL_PAGE_NUMBER(renderer_.getTxtOffset()),
                                            SOL_PAGE_NUMBER(lengthOfFile));
    }

    if (!chapterPagesRemainingString.empty()) {
        footer1TextView_.setText(chapterPagesRemainingString);
    }
    if (!placeStringForBottom.empty()) {
        footer2TextView_.setText(placeStringForBottom);
    }

    SolClock &theClock = SolClock::getInstance();
    auto timeString = theClock.getTimeString();
    footer3TextView_.setText(timeString);

    setNeedsLayout();
}

void NewReadablePopupMenuScreen::preselectIndexAt(int index) { menuView_.setSelectedIndex(index); }

void NewReadablePopupMenuScreen::drawInRect(Rect rect) {
    popupMenuBackground_.fillColor = display_.bg;
    popupMenuBackground_.strokeColor = display_.bg;
    popupMenuForeground_.strokeColor = display_.fg;

    popupMenuBackground_.draw();
    popupMenuForeground_.draw();

    titleView_.draw();
    menuView_.draw();

    footer1TextView_.draw();
    footer2TextView_.draw();
    footer3TextView_.draw();
}

auto NewReadablePopupMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    auto fullFrame = Rect::fromSize(size);

    auto backgroundInset = size.width * 2 / 9;

    // Right now we are forcing this to be an inset from the full height of the size
    // but it would be better if it could layout all the stuff inside it and determine
    // the right height.
    auto backgroundFrame =
        context.setViewFrame(popupMenuBackground_, fullFrame.inset(backgroundInset, 13));
    // Border is inset by 5 pixels on the top and bottom and then 1/4 of the width on the left and
    // right. This will need to be adjusted for different screen resolutions.
    auto foregroundFrame =
        context.setViewFrame(popupMenuForeground_, fullFrame.inset(backgroundInset + 5, 18));
    auto contentFrame = foregroundFrame.inset(5, 10);
    auto y = contentFrame.origin.y;

    auto titleBox = context.setViewFrame(
        titleView_, {{contentFrame.origin.x, y}, contentFrame.size},
        LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignCenterHorizontal);

    y += titleBox.size.height + 12;

    auto menuBox = context.setViewFrame(
        menuView_,
        {{contentFrame.origin.x + contentFrame.size.width / 9, y},
         {contentFrame.size.width - 2 * contentFrame.size.width / 9, contentFrame.size.height}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    y += menuBox.size.height + 2;

    // Nudge down the footers
    if (!footer1TextView_.getText().empty() || !footer2TextView_.getText().empty()) {
        y += 8;
    }

    auto footerRect = Rect({{contentFrame.origin.x, y}, {contentFrame.size.width, 0}});
    if (!footer1TextView_.getText().empty()) {
        auto footerTopRect =
            context.setViewFrame(footer1TextView_, footerRect,
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
        y += footerTopRect.size.height;
        footerRect.origin.y = y;
    }
    if (!footer2TextView_.getText().empty()) {
        auto footerBottomRect =
            context.setViewFrame(footer2TextView_, footerRect,
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
        y += footerBottomRect.size.height;
        footerRect.origin.y = y;
    }
    if (!footer3TextView_.getText().empty()) {
        context.setViewFrame(footer3TextView_, footerRect,
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);
    }

    return backgroundFrame.size;
}

void NewReadablePopupMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    layoutIfNeeded();

    draw();

    if (firstLoad_) {
        firstLoad_ = false;
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true);
        // Redraw but this time without the text (since updateDsplay will clear the screen)
        draw();
    }
    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
};
