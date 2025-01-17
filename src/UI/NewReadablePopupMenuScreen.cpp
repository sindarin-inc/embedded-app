#include "UI/NewReadablePopupMenuScreen.hpp"

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
    if (theClock.isValidTime()) {
        auto timeString = theClock.getTimeString();
        footer3TextView_.setText(timeString);
    }

    setNeedsLayout();
}

void NewReadablePopupMenuScreen::preselectIndexAt(int index) { menuView_.setSelectedIndex(index); }

void NewReadablePopupMenuScreen::drawInRect(SolRect rect) {
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

auto NewReadablePopupMenuScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    auto contentFrame = context.setSublayoutFrame(
        SolRect::fromSize(size), LayoutOptionsSizeToFit | LayoutOptionsAlignCenter,
        [this](LayoutContext &context, SolSize size) -> SolSize {
            int y = 0;
            auto titleBox = context.setViewFrame(titleView_, {{0, y}, size},
                                                 LayoutOptionsSizeToFit | LayoutOptionsAlignTop |
                                                     LayoutOptionsAlignCenterHorizontal);

            y += titleBox.size.height + 12;

            auto menuBox = context.setViewFrame(
                menuView_, {{10, y}, {size.width - 10, size.height - y}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

            y += menuBox.size.height;

            // If there is at least one footer, advance the y
            if (!footer1TextView_.getText().empty() || !footer2TextView_.getText().empty() ||
                !footer3TextView_.getText().empty()) {
                y += 10;
            }

            if (!footer1TextView_.getText().empty()) {
                auto footer1Rect = context.setViewFrame(footer1TextView_, {{0, y}, size},
                                                        LayoutOptionsSizeToFit |
                                                            LayoutOptionsAlignCenterHorizontal);
                y += footer1Rect.size.height;
            }
            if (!footer2TextView_.getText().empty()) {
                auto footer2Rect = context.setViewFrame(footer2TextView_, {{0, y}, size},
                                                        LayoutOptionsSizeToFit |
                                                            LayoutOptionsAlignCenterHorizontal);
                y += footer2Rect.size.height;
            }
            if (!footer3TextView_.getText().empty()) {
                auto footer3Rect = context.setViewFrame(footer3TextView_, {{0, y}, size},
                                                        LayoutOptionsSizeToFit |
                                                            LayoutOptionsAlignCenterHorizontal);
                y += footer3Rect.size.height;
            }

            // We are hardcoding a width here because we have 3 different tabs and each has a
            // differnt width so doing it dynamically would make the bounding box width jump around
            // when you go left and right bewteen tabs. And the y+1 is to give just a bit more space
            // in the bottom for weighting visually.
            return {size.width * 5 / 9 - 18, y + 1};
        });

    auto foregroundFrame = context.setViewFrame(popupMenuForeground_, contentFrame.inset(-5, -10));

    auto backgroundFrame = context.setViewFrame(popupMenuBackground_, foregroundFrame.inset(-5));

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
