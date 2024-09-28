#include "UI/ReadableScreen.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <utility>

#include "Misc/Platform.hpp"
#include "Models/UserReadablePartialChapterSummary.hpp"
#include "UI/Bitmaps/Auto_Pilot_Icons.hpp"
#include "UI/ReadableHookScreens.hpp"
#include "sindarin-debug.h"

// Two minutes per page currently
#define SLOWEST_PAGE_SPEED 120000
// Change speed of speed reading in increments of 2 seconds
#define PAGE_SPEED_INCREMENT 2000

ReadableScreen::ReadableScreen(const std::shared_ptr<RendererStream> &stream,
                               DisplaySystem &display, const std::shared_ptr<Readable> &readable,
                               PreferencesStore &prefs, Telemetry &telemetry,
                               FileSystem &fileSystem, LightingController &lightingController,
                               LocalStore &localStore, Font *font)
    : EinkView(display), stream_(stream),
      renderer_(display, GetTextRectFromPrefs(display, prefs), font), readable_(readable),
      prefs_(prefs), telemetry_(telemetry), fileSystem_(fileSystem),
      lightingController_(lightingController), display_(display), localStore_(localStore) {

    notExist_ = std::make_shared<MsgPopup>(
        this->display_, "This content doesn't have that feature.\nPress any button to continue.");

    notExist_->setBackCallback([this]() {
        this->display_.clearDisplay();
        this->setCurrentScreen(nullptr);
    });

    readablePlacemarkHistoryScreen_ = std::make_shared<ReadablePlacemarkHistoryScreen>(
        this->prefs_, this->fileSystem_, this->readable_, this->renderer_, this->display_,
        this->telemetry_);
    readablePlacemarkHistoryScreen_->setBackCallback([this]() { setCurrentScreen(nullptr); });

    readableDictionaryScreen_ = std::make_shared<ReadableDictionaryScreen>(
        this->prefs_, this->readable_, this->display_, this->renderer_);
    readableDictionaryScreen_->setBackCallback([this]() { setCurrentScreen(nullptr); });

    auto onMainMenu = [this]() {
        hidePopupMenu();
        prefs_.lastReadableId.remove();
        if (!prefs_.cesMode.get().value_or(false)) {
            if (pageSpeed_ != 0) {
                xTimerStop(DisplaySystem::animationTimer, 0);
            }
        }
        setNeedsRerender();
        setCurrentScreen(nullptr);
        back();
    };

    auto onBookmarkToggle = [this]() {
        inBookmark_ = !inBookmark_;
        writeOutBookmarkUpdate();
    };

    auto onReport = [this, &display]() {
        setNeedsRerender();
        auto metrics = readable_->telemetryPlacemarkMetrics();
        metrics["fw"] = SemanticVersionString();
        auto insets = GetTextInsetFromPrefs(display, prefs_);
        metrics["in_l"] = insets.left;
        metrics["in_r"] = insets.right;
        metrics["in_t"] = insets.top;
        metrics["in_b"] = insets.bottom;
        metrics["text_size"] = prefs_.font.get().value_or(PREFS_SOL_SMALL);
        metrics["line_spacing"] = prefs_.lineSpacing.get().value_or(0);
        telemetry_.logMetric(TelemetryField::METRIC_REPORT, readable_->uuid.c_str(), metrics);
        auto reportPopup = std::make_shared<MsgPopup>(
            display, "Thank you, our team will\nreview this page for\nformatting issues.");
        display.clearDisplay();
        this->setCurrentScreen(reportPopup);
        reportPopup->setBackCallback([this, &display]() {
            display.clearDisplay();
            this->setCurrentScreen(nullptr);
        });
    };

    auto popupNavigationFactory = [this, onMainMenu]() -> MenuItemSharedPtrVector {
        auto menuItems = std::make_shared<MenuItemVector>();

        auto menuItem =
            std::make_shared<NewMenuItemSingleLine>("Main Menu", [onMainMenu]() { onMainMenu(); });

        menuItems->emplace_back(menuItem);

        auto stream = renderer_.getStream();
        if (stream && stream->isFor(DocType::EPUB) &&
            std::dynamic_pointer_cast<EPubFile>(stream)->hasToc()) {
            menuItem = std::make_shared<NewMenuItemSingleLine>("Contents", [this]() {
                auto toc = std::make_shared<ReadableTableOfContentsScreen>(
                    prefs_, readable_, renderer_, display_, telemetry_);

                auto stream = renderer_.getStream();
                if (stream->isFor(DocType::EPUB) &&
                    std::dynamic_pointer_cast<EPubFile>(stream)->hasToc()) {
                    toc->loadMenuItems();
                    toc->setNearestChapterSelection(renderer_.getCurrentPlace());
                    toc->setFirstLoad();
                }

                toc->setBackCallback([this]() {
                    this->setCurrentScreen(nullptr);
                    setNeedsRerender();
                });
                this->setCurrentScreen(toc);
            });
        } else {
            menuItem = std::make_shared<NewMenuItemSingleLine>("Seek", [this]() {
                auto readableSeekScreen =
                    std::make_shared<ReadableSeekScreen>(prefs_, readable_, renderer_);
                readableSeekScreen->setFirstLoad();
                readableSeekScreen->setBackCallback([this]() {
                    this->setCurrentScreen(nullptr);
                    setNeedsRerender();
                });
                setCurrentScreen(readableSeekScreen);
            });
        }

        menuItems->emplace_back(menuItem);

        auto menuItemCheckable = std::make_shared<NewMenuItemCheckable>(
            "Autopilot",
            [this]() {
            this->prefs_.autoPilot.set(!this->prefs_.autoPilot.get().value_or(false));
            if (this->prefs_.autoPilot.get().value_or(false)) {
                // Get the speed from the prefs if its there, but if it's 0 or not there,
                // set it to the default speed because they just explicitly turned on
                // autopilot.
                pageSpeed_ = prefs_.autoPilotSpeed.get().value_or(0);
                if (pageSpeed_ == 0) {
                    prefs_.autoPilotSpeed.set(10000);
                    pageSpeed_ = prefs_.autoPilotSpeed.get().value_or(0);
                }
            } else {
                prefs_.autoPilotSpeed.remove();
                pageSpeed_ = 0;
            }
            display_.clearDisplay();
            this->setCurrentScreen(nullptr);
            setNeedsRerender();
            },
            [this]() -> bool { return this->prefs_.autoPilot.get().value_or(false); });

        menuItems->emplace_back(menuItemCheckable);

        auto deleteMenuItem = std::make_shared<NewMenuItemSingleLine>("Delete",
                                                                      [this, onMainMenu]() {
            log_d("Showing delete confirmation!");
            auto lineMenuScreen = std::make_shared<LineMenuScreen>(this->prefs_, this->display_);
            lineMenuScreen->resetMenuItems();
            lineMenuScreen->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Cancel", [this]() {
                log_d("Aborting delete!");
                setCurrentScreen(nullptr);
                back();
            }));
            lineMenuScreen->addMenuItem(
                std::make_shared<NewMenuItemSingleLine>("Delete: I'm sure!", [this, onMainMenu]() {
                    UserReadablesVector readablesToDelete = {};
                    // Create a UserReadable from the readable_ and add it to the
                    // readablesToDelete dynamic_pointer_cast the stream_ to an EPubFile and
                    // close it It's always an EPubFile if they uploaded it to us! Print out the
                    // dynamic type of stream_ to make sure it's always an EPubFile
                    std::shared_ptr<EPubFile> temp = std::dynamic_pointer_cast<EPubFile>(stream_);
                    temp->close();
                    readablesToDelete.emplace_back();
                    readablesToDelete.back().uuid = readable_->shortenedUuid;
                    readablesToDelete.back().isArchived = false;
                    readablesToDelete.back().readable = readable_;
                    localStore_.processReadableDeletion(readablesToDelete, localStore_.books());
                    auto restartSoonPopup = std::make_shared<MsgPopup>(
                        display_, "Book deleted, press \ue000 to restart");
                    display_.clearDisplay();
                    this->setCurrentScreen(restartSoonPopup);
                    setNeedsRerender();
                    restartSoonPopup->setBackCallback([this, onMainMenu]() {
                        display_.clearDisplay();
                        onMainMenu();
                        PowerManager::getInstance().reboot();
                    });
                }));
            setCurrentScreen(lineMenuScreen);
            setNeedsRerender();
        });

        auto archiveMenuItem =
            std::make_shared<NewMenuItemSingleLine>("Archive", [this, onMainMenu]() {
                this->prefs_.readableArchivedChanged.set(readable_->shortenedUuid.c_str(), true);
                onMainMenu();
            });

        auto unarchiveMenuItem = std::make_shared<NewMenuItemSingleLine>("Unarchive", [this]() {
            this->prefs_.readableArchivedChanged.remove(readable_->shortenedUuid.c_str());
            display_.clearDisplay();
            this->setCurrentScreen(nullptr);
            setNeedsRerender();
        });

        // Note the two non-used menu items will get cleaned up at the end of this lambda
        // This seems a bit more readable than doing this math up front or putting all the
        // code above in this if / else nesting.
        if (readable_->appProcessed) {
            // If the readable is from the desktop app, show the delete menu item.
            menuItems->emplace_back(deleteMenuItem);
        } else {
            // Figure out if you should show the Archive menu item or the Unarchive menu
            // item.
            bool archived = prefs_.readableArchivedChanged.get(readable_->shortenedUuid.c_str());
            if (archived) {
                menuItems->emplace_back(unarchiveMenuItem);
            } else {
                menuItems->emplace_back(archiveMenuItem);
            }
        }

        menuItem = std::make_shared<NewMenuItemSingleLine>("Brightness", [this]() {
            auto brightness = std::make_shared<LightingAdjustScreen>(
                display_, prefs_, lightingController_, telemetry_);

            brightness->setBackCallback([this]() {
                this->setCurrentScreen(nullptr);
                setNeedsRerender();
            });
            this->setCurrentScreen(brightness);
        });

        menuItems->emplace_back(menuItem);

        menuItem = std::make_shared<NewMenuItemSingleLine>("Back to Text", [this]() {
            display_.clearDisplay();
            this->setCurrentScreen(nullptr);
            setNeedsRerender();
        });

        menuItems->emplace_back(menuItem);

        return menuItems;
    };

    auto popupContextFactory = [this, onBookmarkToggle, onReport]() -> MenuItemSharedPtrVector {
        auto menuItems = std::make_shared<MenuItemVector>();

        auto menuItem =
            std::make_shared<NewMenuItemSingleLine>("Bookmark", [this, onBookmarkToggle]() {
                onBookmarkToggle();
                display_.clearDisplay();
                this->setCurrentScreen(nullptr);
                setNeedsRerender();
            });

        menuItems->emplace_back(menuItem);

        if (renderer_.getCurrentPlace()->isFor(DocType::EPUB)) {
            auto stream = std::dynamic_pointer_cast<EPubFile>(renderer_.getStream());
            auto placeHref =
                std::dynamic_pointer_cast<EPubPlace>(renderer_.getCurrentPlace())->getHRef();

            if (stream->hasDictionary(placeHref)) {
                menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>("Dictionary",
                                                                                [this]() {
                    auto epubPlace =
                        std::dynamic_pointer_cast<EPubPlace>(renderer_.getCurrentPlace());

                    readableDictionaryScreen_->setPlaceInReadable(epubPlace);
                    readableDictionaryScreen_->setFirstLoad();
                    setCurrentScreen(readableDictionaryScreen_);
                }));
            } else {
                menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
                    "Dictionary", [this]() { setCurrentScreen(notExist_); }));
                log_d("No dictionary for spine %s", placeHref.c_str());
            }

            if (stream->hasSummary(placeHref)) {
                menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>("Summary",
                                                                                [this]() {
                    auto epubPlace =
                        std::dynamic_pointer_cast<EPubPlace>(renderer_.getCurrentPlace());
                    readableSummaryScreen_->setPlaceInReadable(epubPlace);
                    readableSummaryScreen_->setFirstLoad();
                    setCurrentScreen(readableSummaryScreen_);
                }));
            } else {
                log_d("No summary for spine %s", placeHref.c_str());
                menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
                    "Summary", [this]() { setCurrentScreen(notExist_); }));
            }

            if (fileSystem_.exists(readable_->placemarkHistoryFilePath())) {
                menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>("History",
                                                                                [this]() {
                    readablePlacemarkHistoryScreen_->loadMenuItems();
                    readablePlacemarkHistoryScreen_->setFirstLoad();
                    setCurrentScreen(readablePlacemarkHistoryScreen_);
                }));
            } else {
                menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
                    "History", [this]() { setCurrentScreen(notExist_); }));
            }
        } else {
            menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
                "Dictionary", [this]() { setCurrentScreen(notExist_); }));

            menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
                "Summary", [this]() { setCurrentScreen(notExist_); }));

            menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
                "History", [this]() { setCurrentScreen(notExist_); }));
            log_d("Not an EPub, so no history or summary or dictionary");
        }

        menuItem = std::make_shared<NewMenuItemSingleLine>("Report Page", [onReport]() {
            log_d("Reporting a problem with the page");
            onReport();
        });

        menuItems->emplace_back(menuItem);

        menuItem = std::make_shared<NewMenuItemSingleLine>("Back to Text", [this]() {
            display_.clearDisplay();
            this->setCurrentScreen(nullptr);
            setNeedsRerender();
        });

        menuItems->emplace_back(menuItem);

        return menuItems;
    };

    auto popupStyleFactory = [this]() -> MenuItemSharedPtrVector {
        auto menuItems = std::make_shared<MenuItemVector>();

        auto menuItem = std::make_shared<NewMenuItemSingleLine>("Text Size", [this]() {
            auto fontMenu = std::make_shared<ReadingSettingsMenuScreen>(this->display_,
                                                                        "Reading Settings", prefs_);
            fontMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Small", [this]() {
                this->display_.setFont(static_cast<Font *>(&fontFace0));
                this->display_.clearDisplay();
                prefs_.font.set(PREFS_SOL_SMALL);
                this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                               {{"text_size", "small"}});
                getCurrentScreen()->setNeedsRerender();
            }));
            fontMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Medium", [this]() {
                this->display_.setFont(static_cast<Font *>(&fontFace1));
                this->display_.clearDisplay();
                prefs_.font.set(PREFS_SOL_MEDIUM);
                this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                               {{"text_size", "medium"}});
                getCurrentScreen()->setNeedsRerender();
            }));
            fontMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Large", [this]() {
                this->display_.setFont(static_cast<Font *>(&fontFace2));
                this->display_.clearDisplay();
                prefs_.font.set(PREFS_SOL_LARGE);
                this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                               {{"text_size", "large"}});
                getCurrentScreen()->setNeedsRerender();
            }));
            int menuIndex = 0;
            if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_SMALL) {
                menuIndex = 0;
            } else if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_MEDIUM) {
                menuIndex = 1;
            } else if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_LARGE) {
                menuIndex = 2;
            }
            fontMenu->setSelectedIndex(menuIndex);

            setCurrentScreen(fontMenu);
            fontMenu->setBackCallback([this]() {
                Font *font = FontFromPrefs(prefs_.font.get().value_or(PREFS_SOL_SMALL));
                if (font) {
                    this->renderer_.setFont(font);
                }
                setCurrentScreen(nullptr);
            });
        });

        menuItems->emplace_back(menuItem);

        menuItem = std::make_shared<NewMenuItemSingleLine>("Move Text", [this]() {
            log_d("Loading adjust offset");
            auto offsetAdjust =
                std::make_shared<OffsetAdjustScreen>(this->display_, prefs_, this->telemetry_);
            this->setCurrentScreen(offsetAdjust);
            log_d("setting up callback");
            offsetAdjust->setBackCallback([this]() {
                this->renderer_.setRect(GetTextRectFromPrefs(this->display_, this->prefs_));
                setCurrentScreen(nullptr);
            });
            log_d("set up callback");
        });

        menuItems->emplace_back(menuItem);

        menuItem = std::make_shared<NewMenuItemSingleLine>("Resize Page", [this]() {
            log_d("Loading resize box");
            auto resizeBox =
                std::make_shared<ResizeBoxScreen>(this->display_, prefs_, this->telemetry_);
            this->setCurrentScreen(resizeBox);
            log_d("setting up callback");
            resizeBox->setBackCallback([this]() {
                this->renderer_.setRect(GetTextRectFromPrefs(this->display_, this->prefs_));
                setCurrentScreen(nullptr);
            });
            log_d("set up callback");
        });

        menuItems->emplace_back(menuItem);

        menuItem = std::make_shared<NewMenuItemSingleLine>("Line Spacing", [this]() {
            auto lineSpacingScreen =
                std::make_shared<LineSpacingScreen>(this->display_, prefs_, this->telemetry_);
            this->setCurrentScreen(lineSpacingScreen);
            lineSpacingScreen->setBackCallback([this]() { setCurrentScreen(nullptr); });
        });

        menuItems->emplace_back(menuItem);

        auto menuItemCheckable = std::make_shared<NewMenuItemCheckable>(
            "Dark Mode",
            [this]() {
            this->prefs_.darkMode.set(!this->prefs_.darkMode.get().value_or(false));
            // Reserved for future use? Potentially lighting up and down?
            this->display_.flipInversion();
            // When we invert, we should do a full clear of the display to make sure
            // the blacks are really gone.
            this->display_.clearDisplay();
            prefs_.invertSetting.set(this->display_.getInversion());
            display_.clearDisplay();
            setNeedsRerender();
            setFirstLoad();
            },
            [this]() -> bool { return this->prefs_.darkMode.get().value_or(false); });

        menuItems->emplace_back(menuItemCheckable);

        menuItem = std::make_shared<NewMenuItemSingleLine>("Back to Text", [this]() {
            display_.clearDisplay();
            this->setCurrentScreen(nullptr);
            setNeedsRerender();
        });

        menuItems->emplace_back(menuItem);

        return menuItems;
    };

    popupMenuScreen_ = std::make_shared<NewReadablePopupMenuScreen>(
        display, prefs, fileSystem, readable, lightingController, renderer_, telemetry_, localStore,
        stream_);

    popupMenuScreen_->addTab("Popup", popupNavigationFactory);

    popupMenuScreen_->addTab("Context", popupContextFactory);

    popupMenuScreen_->addTab("Style", popupStyleFactory);

    popupMenuScreen_->populateMenuView();

    readableSummaryScreen_ =
        std::make_shared<ReadableSummaryScreen>(prefs, fileSystem, readable, display, renderer_);

    renderer_.setStream(stream);

    std::string thePlace = prefs_.readablePlace.get(readable_->shortenedUuid.c_str());
    if (prefs_.cesMode.get().value_or(false)) {
        // For CES mode we intentionally want to ignore the saved place
        thePlace = "";
    }

    log_d("readablePlace: \"%s\"", thePlace.c_str());
    readable_->place = Place::factory(thePlace, stream->getDocType());

    // for the animation bits since we may end up coming back to this constructor after
    // being interrupted.
    newPlace_ = readable_->place;
    atEnd_ = this->checkAtEnd();
    pageSpeed_ = prefs_.autoPilotSpeed.get().value_or(0);

    log_d("Found place for %s = %s %s", readable_->shortenedUuid.c_str(),
          readable_->place->str().c_str(), atEnd_ ? "(at end)" : "");

    popupMenuScreen_->setBackCallback([this]() {
        autoPilotForceFull_ = true;
        this->setCurrentScreen(nullptr);
    });

    readableSummaryScreen_->setBackCallback([this, &display]() {
        renderedPartialSummary_ = true;
        display.clearDisplay();
        this->setCurrentScreen(nullptr);
    });
}

void ReadableScreen::setCurrentScreen(const std::shared_ptr<Screen> &screen) {
    if (currentScreen_) {
        currentScreen_->setFirstLoad();
    }
    if (screen) {
        screen->setNeedsRerender();
    } else {
        renderPopup_ = false;
        firstLoad_ = true;
        setNeedsRerender();
    }
    currentScreen_ = screen;
}

auto ReadableScreen::layout(LayoutContext &context, Size size) -> Size { return size; }

void ReadableScreen::drawInRect(Rect rect) {}

// Once we have a real database, this will all be replaced.
// For now, since we assume that there won't be TOO many bookmarks, we simply
// start from the first bookmark action (on or off) that the user did and replay
// every bookmark action to determine if the current page is bookmarked.
//
// We assume if the midpoint of the current page is within a range of a previously
// defined bookmark, that counts. Of course, if font changes or page size changes,
// there could be cases where you have as single bookmark that shows up on multiple
// pages, but I think that's fine for now.
//
void ReadableScreen::updateBookmarkStatus() {
    inBookmark_ = false;
    SFile bookmarkFile = fileSystem_.open(readable_->bookmarkFilePath(), FILE_READ, false);
    if (!bookmarkFile) {
        log_i("Failed to open bookmark file: %s", readable_->bookmarkFilePath().c_str());
        return;
    }

    LocalBookmark bookmark;
    while (ReadOneBookmark(bookmarkFile, bookmark)) {
        // A decent heuristic for now. If the midpoint of the bookmark is within
        // the current page range, then turn the bookmark on for this page.

        // Ensure that the current chapter matches the place in the bookmark
        auto isEpub = bookmark.firstPlace->isFor(DocType::EPUB);
        if (isEpub) {
            if (std::dynamic_pointer_cast<EPubPlace>(bookmark.firstPlace)->getHRef() !=
                std::dynamic_pointer_cast<EPubPlace>(readable_->place)->getHRef()) {
                continue;
            }
        }

        uint32_t currentPlace = readable_->place->getOffset();
        uint32_t bookmarkAveragePlace;
        uint16_t pageLength = renderer_.getTotalLengthOfLines();
        if (isEpub && (std::dynamic_pointer_cast<EPubPlace>(bookmark.firstPlace)->getHRef() ==
                       std::dynamic_pointer_cast<EPubPlace>(bookmark.lastPlace)->getHRef())) {
            bookmarkAveragePlace =
                bookmark.firstPlace->getOffset() +
                (bookmark.lastPlace->getOffset() - bookmark.firstPlace->getOffset()) / 2;
        } else {
            bookmarkAveragePlace = bookmark.firstPlace->getOffset() + (pageLength / 2);
        }

        if (bookmarkAveragePlace >= currentPlace &&
            bookmarkAveragePlace <= (currentPlace + pageLength)) {
            if (bookmark.status == BOOKMARK_ON) {
                inBookmark_ = true;
            } else {
                inBookmark_ = false;
            }
        }
    }
    bookmarkFile.close();
}

auto ReadableScreen::getScreenCurrentCharOffset() -> uint32_t {
    auto epubFile = std::dynamic_pointer_cast<EPubFile>(renderer_.getStream());
    auto epubPlace = std::dynamic_pointer_cast<EPubPlace>(readable_->place);
    if ((epubFile != nullptr) && (epubPlace != nullptr)) {
        return epubFile->findCharOffsetAtFileOffset(epubPlace->getHRef(), epubPlace->getOffset());
    } else {
        return 0;
    }
}

void ReadableScreen::writeOutBookmarkUpdate() {
    // create log directory in filesystem if it doesn't exist
    if (!fileSystem_.createDir("/bookmarks")) {
        log_e("could not create /bookmarks directory");
        return;
    }

    SFile bookmarkFile = fileSystem_.open(readable_->bookmarkFilePath(), FILE_APPEND, true);

    if (!bookmarkFile) {
        log_i("Failed to open bookmark file: %s", readable_->bookmarkFilePath().c_str());
        return;
    }

    std::string plusOrMinus = inBookmark_ ? "+ " : "- ";
    plusOrMinus += readable_->place->str() + " " + newPlace_->str() + "\n";
    bookmarkFile.write(reinterpret_cast<const uint8_t *>(plusOrMinus.c_str()),
                       plusOrMinus.length());
    bookmarkFile.flush();
    bookmarkFile.close();

    prefs_.readableBookmarksChanged.set(readable_->shortenedUuid.c_str(), true);
}

// Since this happens in a couple of places (animation as well as button press),
// we have a helper method to do the work.
void ReadableScreen::turnPage() {
    if (atEnd_) {
        if (prefs_.emcMode.get().value_or(false)) {
            log_d("Looping back to the beginning of Readable for EMC testing mode");
            // Loop back to the beginning
            readable_->placePrev->setOffset(0);
            readable_->place->setOffset(0);
            return;
        }
        showPopupMenu();
        // See the layout in popupNavigationFactory to ensure this is still the right setting.
        popupMenuScreen_->preselectIndexAt(3);
    } else {
        readable_->placePrev = readable_->place;
        readable_->place = newPlace_;
        prefs_.readablePlace.set(readable_->shortenedUuid.c_str(), readable_->place->str());
        prefs_.readablePlaceChanged.set(readable_->shortenedUuid.c_str(), true);
        prefs_.lastReadTime.set(readable_->shortenedUuid.c_str(),
                                SolClock::getInstance().getEpochTime());
        speedLinesToSkip_ = 0;
    }

    uint32_t wc = prefs_.wordCount.get().value_or(0);
    wc = wc + renderer_.getCurrentPageWordCount();
    prefs_.wordCount.set(wc);
}

auto ReadableScreen::checkAtEnd() -> bool { return renderer_.atEndOfFile(); }

void ReadableScreen::render(DisplaySystem &display) {

    // Rendering the popup is a special case. We first re-draw the text onto the background
    // (since it would have been cleared at the end of the last updateDisplay), then we render
    // the popup over it, then we draw that as a partial refresh. Then we only render the popup
    // (no text) and update again as AU4. That causes the text to be slightly faded out, but the
    // popup menu to be very clear. NOTE: We might consider in the future, not always clearing
    // the display (`fillScreen(bg);`) after updateDisplay. Then we could simply draw over
    // existing screen contents without rerendering it. But that would be a pretty global
    // change, so we'll leave it for now.
    bool renderPopupBackground = currentScreen_ == popupMenuScreen_ && renderPopup_;

    if (currentScreen_ && !renderPopupBackground) {
        currentScreen_->render(display);
        return;
    }

    if (!hookScreens_.empty()) {
        auto &hookScreen = hookScreens_[0];
        hookScreen->setNeedsRerender();
        setCurrentScreen(hookScreen);
        hookScreen->setBackCallback([this, hookScreen]() {
            this->setNeedsRerender();
            setCurrentScreen(nullptr);
            this->hookScreens_.erase(this->hookScreens_.begin());
        });
        hookScreen->render(display);
        return;
    }

    setFrame(defaultFrameRect());

    prefs_.lastReadableId.set(readable_->uuid.c_str());
    telemetry_.logMetric(TelemetryField::METRIC_READABLE_PLACE, readable_->shortenedUuid.c_str(),
                         readable_->telemetryPlacemarkMetrics());

    display.setFont(static_cast<Font *>(&fontFace0));

    // If there is a partial chapter summary, open that instead
    if (firstLoad_ && !renderedPartialSummary_ &&
        !prefs_.noShowPartialChapterSummary.get().value_or(false)) {
        auto partialChapterSummary =
            UserReadablePartialChapterSummary::loadForReadable(fileSystem_, readable_);
        if (partialChapterSummary) {
            auto epubPlace = std::dynamic_pointer_cast<EPubPlace>(readable_->place);
            if (*partialChapterSummary->ePubPlace == *epubPlace) {
                log_d("Rendering partial chapter summary for %s, place %s",
                      readable_->shortenedUuid.c_str(), readable_->place->str().c_str());
                readableSummaryScreen_->setPlaceInReadable(epubPlace);
                readableSummaryScreen_->setFirstLoad();
                setCurrentScreen(readableSummaryScreen_);
                readableSummaryScreen_->render(display);
                return;
            } else {
                log_i("Partial chapter summary does not match current place, not showing: %s vs %s",
                      readable_->place->str().c_str(),
                      partialChapterSummary->ePubPlace->str().c_str());
            }
        }
    }

    bool emcMode = prefs_.emcMode.get().value_or(false);

    // If it's the first read of the book, show the help screen.
    // Then ignore the next press of the button
    if (!emcMode && !prefs_.firstRead.get().value_or(false)) {
        std::string bookOrArticle = (readable_->isArticle()) ? "article" : "book";
        std::string message = "Welcome to your first " + bookOrArticle +
                              "!\n\n"
                              "\ue000  or  \uE002   Next Page\n\n"
                              "\ue001   Previous Page\n\n"
                              "\ue003  or  \ue004   Popup Menu\n\n "
                              "Press any button to start...";
        MsgPopup::showTemporaryMessage(display, message);
        prefs_.firstRead.set(true);
        showingFirstTimeView_ = true;
        return;
    }

    if (!emcMode && !prefs_.firstAutopilotRead.get().value_or(false) && (pageSpeed_ != 0)) {
        // We need to make sure this goes onto the second page because of a bug in the renderer.
        std::string message = "Welcome to Autopilot!\n\n"
                              "\ue003  or  \ue004   Popup Menu\n\n"
                              "\ue001   Slow Down\n\n"
                              "\ue002   Speed Up\n\n"
                              "\ue000   Pause/unpause\n\n"
                              "Press any button to start...";

        MsgPopup::showTemporaryMessage(display, message);
        prefs_.firstAutopilotRead.set(true);
        // Make sure the next screen is a full refresh
        display.clearDisplay();
        showingFirstTimeView_ = true;
        return;
    }

    uint8_t darkPicturePercent = 0;

    if (pageSpeed_ == 0) {
        // Regular Reading
        bool retrieveMenuParameters = true;
        do {
            std::tie(newPlace_, darkPicturePercent) =
                renderer_.renderPage(readable_->place, 0, false, retrieveMenuParameters);
            if (retrieveMenuParameters) {
                const MenuParameters &menuParameters = renderer_.getMenuParameters();
                if (!menuParameters.empty()) {
                    // Note that we can't guarantee the order in which the hooks
                    // will be parsed out and executed. So if you have multiple
                    // hooks in one place, just keep that in mind.
                    log_d("Menu Parameters:");
                    for (auto &param : menuParameters) {
                        log_d("Creating hook screen %s: %s", param.first.c_str(),
                              param.second.c_str());
                        hookScreens_.push_back(ReadableHookScreens::getNewHookScreen(
                            param.first, display_, prefs_, telemetry_));
                    }
                    retrieveMenuParameters = false;
                    continue;
                }
            }

            break;
        } while (true);

        // log_w("Char Offset: %" PRIu32, getScreenCurrentCharOffset());
        atEnd_ = checkAtEnd();
    } else {
        // Autopilot Reading
        if (speedLinesToSkip_ >= renderer_.getNumRenderedLines()) {
            turnPage();
        } else {
            speedLinesToSkip_++;
        }

        std::tie(newPlace_, darkPicturePercent) =
            renderer_.renderPage(readable_->place, speedLinesToSkip_);
        atEnd_ = checkAtEnd();

        // If the line is blank, we want to skip it. Also, skip multiple newlines in a row.
        while ((renderer_.getLengthOfLine(speedLinesToSkip_) == 0) &&
               speedLinesToSkip_ < renderer_.getNumRenderedLines()) {
            speedLinesToSkip_++;
        }

        // Only if we didn't end on a newline. Othwerise wait for next render to turn page.
        if (speedLinesToSkip_ != renderer_.getNumRenderedLines()) {
            auto totalLength = renderer_.getTotalLengthOfLines();
            nextInterval_ =
                (totalLength != 0)
                    ? pageSpeed_ * renderer_.getLengthOfLine(speedLinesToSkip_) / totalLength
                    : 0;
            if (nextInterval_ == 0) {
                nextInterval_ = 1;
            }
        }

        // This happens if the very first screen rendered is an image where there's only 1 "line"
        if (nextInterval_ == 0) {
            // Let them look at the image for 1 second and then move on.
            nextInterval_ = 1000;
        }

        // Give the user extra time for the first line.
        if (speedLinesToSkip_ == 0) {
            nextInterval_ *= 2;
        }

        log_d("Setting next interval: %lu", nextInterval_);

        setNeedsRerender();
    }

    updateBookmarkStatus();

    if (inBookmark_) {
        int leftSide = display.width() - 26;
        log_d("Drawing bookmark!");
        display.fillRect(leftSide + 1, 0, 13, 55, display.fg);
        display.fillTriangle(leftSide + 1, 55, leftSide + 13, 55, leftSide + 7, 50, display.bg);
    }

    // Only show the autopilot icon on page turns or if speed was changed -- let it fade out
    // with the rest of the text
    if ((pageSpeed_ != 0) && (speedLinesToSkip_ == 0)) {
        int screenWidth = display.width();
        display.drawXBitmap(screenWidth + 2, 0, autoPilotBits, auto_pilot_width, auto_pilot_height,
                            display.fg);
    }

    log_d("%s is the new place in the text %s", newPlace_->str().c_str(), atEnd_ ? "(at end)" : "");

    if (renderPopupBackground) {
        popupMenuScreen_->render(display);
    } else if ((pageSpeed_ != 0) && (speedLinesToSkip_ > 0)) {
        if (autoPilotForceFull_) {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
            autoPilotForceFull_ = false;
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FAST_ORIGINAL_AU4, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        }
    } else {
        if (prefs_.fastTransitions.get().value_or(false)) {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        }
    }

    if (pageSpeed_ != 0) {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        if (!autoPilotPause_) {
            xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
        }
    }

    if (emcMode) {
        // Must stop the timer because ChangePeriod starts it.
        xTimerStop(DisplaySystem::animationTimer, 0);
        // Change page every 30 seconds
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(30000), 0);
        turnPage();
    }

    renderPopup_ = false;

    // Reset the lastArrowPress so we don't show it again until another key is pressed (for
    // autopilot mostly)
    lastArrowPress_ = REMOTE_NONE;

    firstLoad_ = false;
}

void ReadableScreen::showPopupMenu() {
    renderPopup_ = true;
    popupMenuScreen_->setUpPopupMenu();
    popupMenuScreen_->regenerateCurrentTab();
    setCurrentScreen(popupMenuScreen_);
}

void ReadableScreen::hidePopupMenu() { setCurrentScreen(nullptr); }

void ReadableScreen::processCommandDemoMode(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT:
        numUps_ = 0;
        processCommandNormal(command);
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        numUps_ = 0;
        turnPage();
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
        numUps_ = 0;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::UP:
        numUps_++;
        if (numUps_ > 3) {
            numUps_ = 0;
            prefs_.cesMode.set(false);
            setNeedsRerender();
            display_.clearDisplay();
            display_.clearFrameBuffer();
            display_.clearDontChangeRefreshType();
            back();
        }
        break;
    case RemoteCommand::ANIMATION:
    default:
        break;
    }
}

void ReadableScreen::backUpAutopilot(int numLines) {
    // When entering the popup menu from autopilot, go back up to some lines so that coming
    // out the user can not be behind.
    speedLinesToSkip_ = std::max(0, static_cast<int>(speedLinesToSkip_) - numLines);
}

void ReadableScreen::processCommandAutoPilot(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT:
        pageSpeed_ = std::min(pageSpeed_ + PAGE_SPEED_INCREMENT, SLOWEST_PAGE_SPEED);
        prefs_.autoPilotSpeed.set(pageSpeed_);
        log_d("Slowing down: New Page Speed is %u", pageSpeed_);
        // If they want to slow down, given them a couple lines back.
        backUpAutopilot(2);
        break;
    case RemoteCommand::ENTER:
        // If we aren't pause, pause.
        // If we are paused, start the timer again.
        log_d("Pausing or unpausing auto pilot");
        if (!autoPilotPause_) {
            xTimerStop(DisplaySystem::animationTimer, 0);
        } else {
            xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
        }
        autoPilotPause_ = !autoPilotPause_;
        break;

    case RemoteCommand::DOWN:
    case RemoteCommand::BACK:
    case RemoteCommand::UP:
        // Given them a couple lines back on popup menu for context
        backUpAutopilot(2);
        xTimerStop(DisplaySystem::animationTimer, 0);
        processCommandNormal(command);
        break;
    case RemoteCommand::RIGHT:
        pageSpeed_ = std::max(pageSpeed_ - PAGE_SPEED_INCREMENT, 1);
        log_d("Speeding up: New Page Speed is %u", pageSpeed_);
        prefs_.autoPilotSpeed.set(pageSpeed_);
        backUpAutopilot(1);
        break;

    case RemoteCommand::ANIMATION:
    default:
        break;
    }

    setNeedsRerender();
}

void ReadableScreen::processCommandNormal(RemoteCommand command) {
    // Go ahead and begin powering on the screen since in all cases we trigger a re-render
    display_.beginPowerOn();

    switch (command) {
    case RemoteCommand::LEFT:
        readable_->place = renderer_.getPreviousPlace();
        prefs_.readablePlace.set(readable_->shortenedUuid.c_str(), readable_->place->str());
        prefs_.readablePlaceChanged.set(readable_->shortenedUuid.c_str(), true);
        prefs_.lastReadTime.set(readable_->shortenedUuid.c_str(),
                                SolClock::getInstance().getEpochTime());
        break;
    case RemoteCommand::ENTER:
        if (prefs_.fastBookmarkCenter.get().value_or(false)) {
            inBookmark_ = !inBookmark_;
            writeOutBookmarkUpdate();
        } else {
            turnPage();
        }
        break;
    case RemoteCommand::DOWN:
        showPopupMenu();
        break;
    case RemoteCommand::RIGHT:
        if (prefs_.fastBookmarkRight.get().value_or(false)) {
            inBookmark_ = !inBookmark_;
            writeOutBookmarkUpdate();
        } else {
            turnPage();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::UP:
        showPopupMenu();
        break;

    case RemoteCommand::ANIMATION:
    default:
        break;
    }

    setNeedsRerender();
}

void ReadableScreen::processCommand(RemoteCommand command) {
    if (currentScreen_) {
        // Save off this pointer so it doesn't get cleaned up by the backCallback when
        // we set currentScreen_ to nullptr
        auto screen = currentScreen_;
        screen->processCommand(command);
        // currentScreen could be reset to null here
        if (currentScreen_ && currentScreen_->needsRerender()) {
            setNeedsRerender();
        }
        return;
    }

    // This is the first time view and so this command was from the "press any button" MsgPopup
    // So we simply rerender the page without the popup and go about our business.
    if (showingFirstTimeView_) {
        showingFirstTimeView_ = false;
        setNeedsRerender();
        return;
    }

    switch (command) {
    case RemoteCommand::LEFT:
        lastArrowPress_ = REMOTE_LEFT;
        break;
    case RemoteCommand::RIGHT:
        lastArrowPress_ = REMOTE_RIGHT;
        break;
    case RemoteCommand::ENTER:
        lastArrowPress_ = REMOTE_CENTER;
        break;
    case RemoteCommand::DOWN:
        lastArrowPress_ = REMOTE_DOWN;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::UP:
        lastArrowPress_ = REMOTE_UP;
        break;
    default:
        break;
    }
    // Similarly, for demo mode, we want a simple set of commands.
    if (prefs_.cesMode.get().value_or(false)) {
        processCommandDemoMode(command);
        return;
    }

    // Then we move on to autopilot mode and regular mode.
    if (pageSpeed_ != 0) {
        processCommandAutoPilot(command);
    } else {
        processCommandNormal(command);
    }
}
