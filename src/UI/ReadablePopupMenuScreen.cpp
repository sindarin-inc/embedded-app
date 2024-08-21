#include "ReadablePopupMenuScreen.hpp"

#include "UI/MsgPopup.hpp"

void ReadablePopupMenuScreen::setUpPopupMenu() {
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
        popupMenuView_.setFooter1Text(chapterPagesRemainingString);
    }
    if (!placeStringForBottom.empty()) {
        popupMenuView_.setFooter2Text(placeStringForBottom);
    }

    SolClock &theClock = SolClock::getInstance();
    auto timeString = theClock.getTimeString();
    popupMenuView_.setFooter3Text(timeString);

    // Always populate the popup menu because each chapter may have different entries (dictionary,
    // summary)
    populatePopupMenu();

    if (readable_->appProcessed) {
        // If the readable is from the desktop app, show the delete menu item.
        deleteMenuItemView_->setHidden(false);
        archiveMenuItemView_->setHidden(true);
        unarchiveMenuItemView_->setHidden(true);
    } else {
        // Figure out if you should show the Archive menu item or the Unarchive menu item.
        bool archived = prefs_.readableArchivedChanged.get(readable_->shortenedUuid.c_str());
        deleteMenuItemView_->setHidden(true);
        if (archived) {
            archiveMenuItemView_->setHidden(true);
            unarchiveMenuItemView_->setHidden(false);
        } else {
            archiveMenuItemView_->setHidden(false);
            unarchiveMenuItemView_->setHidden(true);
        }
    }

    popupMenuView_.setNeedsLayout();
    setNeedsLayout();
    setFirstLoad();
}

void ReadablePopupMenuScreen::addMenuItem(const MenuItem &newItem) {
    Menu::addMenuItem(newItem);
    auto menuItemView = std::make_shared<MenuItemView>(display_, newItem);
    menuItemView->setCircleIndicator(true);
    popupMenuView_.menuItemViews.emplace_back(menuItemView);
}

void ReadablePopupMenuScreen::populatePopupMenu() {
    resetMenuItems();

    addMenuItem(MenuItem("Main Menu", [this]() { onMainMenu(); }));
    addMenuItem(MenuItem("Bookmark", [this]() {
        onBookmarkToggle();
        back();
    }));
    addMenuItem(MenuItem("Brightness", [this]() {
        log_d("Setting current screen to brightness screen");
        lightingAdjustScreen_.setFirstLoad();
        setCurrentScreen(lightingAdjustScreen_);
    }));

    addMenuItem(MenuItem("Delete", [this]() {
        log_d("Showing delete confirmation!");
        lineMenuScreen_.resetMenuItems();
        lineMenuScreen_.addMenuItem(MenuItem("Cancel", [this]() {
            log_d("Aborting delete!");
            setCurrentScreen(*this);
            back();
        }));
        lineMenuScreen_.addMenuItem(MenuItem("Delete: I'm sure!", [this]() {
            UserReadablesVector readablesToDelete = {};
            // Create a UserReadable from the readable_ and add it to the readablesToDelete
            // dynamic_pointer_cast the stream_ to an EPubFile and close it
            // It's always an EPubFile if they uploaded it to us! Print out the dynamic type of
            // stream_ to make sure it's always an EPubFile
            std::shared_ptr<EPubFile> temp = std::dynamic_pointer_cast<EPubFile>(stream_);
            temp->close();
            readablesToDelete.emplace_back();
            readablesToDelete.back().uuid = readable_->shortenedUuid;
            readablesToDelete.back().isArchived = false;
            readablesToDelete.back().readable = readable_;
            localStore_.processReadableDeletion(readablesToDelete, localStore_.books());
            auto *restartSoonPopup =
                new MsgPopup(display_, "Book deleted, press \ue000 to restart");
            display_.clearDisplay();
            this->setCurrentScreen(*restartSoonPopup);
            setNeedsRerender();
            restartSoonPopup->setBackCallback([this, restartSoonPopup]() {
                display_.clearDisplay();
                onMainMenu();
                delete restartSoonPopup;
                PowerManager::getInstance().reboot();
            });
        }));
        setCurrentScreen(lineMenuScreen_);
        setNeedsRerender();
    }));

    deleteMenuItemView_ = popupMenuView_.menuItemViews.back();

    addMenuItem(MenuItem("Archive", [this]() {
        prefs_.readableArchivedChanged.set(readable_->shortenedUuid.c_str(), true);
        onMainMenu();
    }));
    archiveMenuItemView_ = popupMenuView_.menuItemViews.back();

    addMenuItem(MenuItem("Unarchive", [this]() {
        this->prefs_.readableArchivedChanged.remove(readable_->shortenedUuid.c_str());
        back();
    }));
    unarchiveMenuItemView_ = popupMenuView_.menuItemViews.back();

    addMenuItem(MenuItem("Report page", [this]() {
        log_d("Reporting a problem with the page");
        onReport();
    }));

    if (renderer_.getCurrentPlace()->isFor(DocType::EPUB)) {
        auto stream = std::dynamic_pointer_cast<EPubFile>(renderer_.getStream());
        auto placeHref =
            std::dynamic_pointer_cast<EPubPlace>(renderer_.getCurrentPlace())->getHRef();

        if (fileSystem_.exists(readable_->placemarkHistoryFilePath())) {
            addMenuItem(MenuItem("History", [this]() {
                readablePlacemarkHistoryScreen_.loadMenuItems();
                readablePlacemarkHistoryScreen_.setFirstLoad();
                setCurrentScreen(readablePlacemarkHistoryScreen_);
            }));
        }

        if (stream->hasSummary(placeHref)) {
            addMenuItem(MenuItem("Summary", [this]() {
                auto epubPlace = std::dynamic_pointer_cast<EPubPlace>(renderer_.getCurrentPlace());
                readableSummaryScreen_.setPlaceInReadable(epubPlace);
                readableSummaryScreen_.setFirstLoad();
                setCurrentScreen(readableSummaryScreen_);
            }));
        } else {
            log_d("No summary for spine %s", placeHref.c_str());
        }

        if (stream->hasDictionary(placeHref)) {
            addMenuItem(MenuItem("Dictionary", [this]() {
                auto epubPlace = std::dynamic_pointer_cast<EPubPlace>(renderer_.getCurrentPlace());
                readableDictionaryScreen_.setPlaceInReadable(epubPlace);
                readableDictionaryScreen_.setFirstLoad();
                setCurrentScreen(readableDictionaryScreen_);
            }));
        } else {
            log_d("No dictionary for spine %s", placeHref.c_str());
        }
    } else {
        log_d("Not an EPub, so no summary or dictionary");
    }

    auto stream = renderer_.getStream();
    if (stream->isFor(DocType::EPUB) && std::dynamic_pointer_cast<EPubFile>(stream)->hasToc()) {
        addMenuItem(MenuItem("Contents", [this]() {
            readableTableOfContentsScreen_.loadMenuItems();
            readableTableOfContentsScreen_.setNearestChapterSelection(renderer_.getCurrentPlace());
            readableTableOfContentsScreen_.setFirstLoad();
            setCurrentScreen(readableTableOfContentsScreen_);
        }));
    } else {
        addMenuItem(MenuItem("Seek", [this]() {
            readableSeekScreen_.setFirstLoad();
            setCurrentScreen(readableSeekScreen_);
        }));
    }
}

void ReadablePopupMenuScreen::preselectArchiveOrDeleteMenuItem() {
    bool archived = prefs_.readableArchivedChanged.get(readable_->shortenedUuid.c_str());
    int index = 0;
    for (auto &menuItemView : popupMenuView_.menuItemViews) {
        if (menuItemView == (readable_->localOnly
                                 ? (deleteMenuItemView_)
                                 : (archived ? unarchiveMenuItemView_ : archiveMenuItemView_))) {
            setCurrSelection(index);
            break;
        }
        index++;
    }
}

void ReadablePopupMenuScreen::preselectFirstItem() { setCurrSelection(0); }

void ReadablePopupMenuScreen::preselectLastItem() { setCurrSelection(numMenuItems_ - 1); }

void ReadablePopupMenuScreen::processCommand(RemoteCommand command) {
    if (currentScreen_ != this) {
        currentScreen_->processCommand(command);
        return;
    }

    switch (command) {
    case RemoteCommand::LEFT:
        back();
        break;
    case RemoteCommand::ENTER:
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
    case RemoteCommand::DOWN:
        do {
            nextSelection();
            setNeedsRerender();
        } while (popupMenuView_.menuItemViews[getCurrSelection()]->isHidden());
        break;
    case RemoteCommand::RIGHT:
    case RemoteCommand::BACK:
        back();
        break;
    case RemoteCommand::UP:
        do {
            prevSelection();
            setNeedsRerender();
        } while (popupMenuView_.menuItemViews[getCurrSelection()]->isHidden());
        break;
    case RemoteCommand::ANIMATION:
    default:
        break;
    }
}

void ReadablePopupMenuScreen::drawInRect(Rect rect) {}

auto ReadablePopupMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    auto rect = Rect({{0, 0}, size});
    context.setViewFrame(popupMenuView_, rect, LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
    return size;
}

void ReadablePopupMenuScreen::render(DisplaySystem &display) {
    if (currentScreen_ != this) {
        currentScreen_->render(display);
        return;
    }

    setFrame(defaultFrameRect());
    layoutIfNeeded();

    if (firstLoad_) {
        // This displays the popup with a pre-AU4 refresh without the bullet
        // indiciating which menu item is selected.
        for (auto &menuItemView : popupMenuView_.menuItemViews) {
            menuItemView->setSelected(false);
        }

        popupMenuView_.draw();

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        firstLoad_ = false;
    } else {
        // NOTE: This Screen is quirky since it is rendered directly from ReadableScreen. The
        // first time it is rendered, it will be rendered twice from ReadableScreen, once with
        // the text in the background (using PARTIAL) and once without the text in the
        // background (but using AU4). This creates the effect of the popup happening on top of
        // the text.
        std::shared_ptr<MenuItemView> selectedMenuItemView =
            popupMenuView_.menuItemViews[getCurrSelection()];
        for (auto &menuItemView : popupMenuView_.menuItemViews) {
            if (menuItemView && menuItemView == selectedMenuItemView) {
                menuItemView->setSelected(true);
            } else {
                menuItemView->setSelected(false);
            }
        }

        popupMenuView_.draw();

        display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);
    }
};

void ReadablePopupMenuScreen::showTemporaryMessage(DisplaySystem &display,
                                                   const std::string &message, int duration) {
    display.setFont(display.style.getFontFamily().settingsFont);
    display.drawSingleLineOfCenteredText(message);
    display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    Delay(duration);
}
