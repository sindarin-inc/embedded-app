#include <utility>

#include "Misc/PowerManager.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Renderers/RendererStream.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/ContrastAdjustScreen.hpp"
#include "UI/FirstHookTestScreen.hpp"
#include "UI/LightingAdjustScreen.hpp"
#include "UI/LineMenuScreen.hpp"
#include "UI/LineSpacingScreen.hpp"
#include "UI/Menu/NewMenuItemCheckable.hpp"
#include "UI/Menu/NewMenuItemSingleLine.hpp"
#include "UI/Menu/ReadingSettingsMenuScreen.hpp"
#include "UI/MsgPopup.hpp"
#include "UI/NewReadablePopupMenuScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/ReadableDictionaryScreen.hpp"
#include "UI/ReadablePlacemarkHistoryScreen.hpp"
#include "UI/ReadableSummaryScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/Screen.hpp"
#include "Version.hpp"
#include "config.h"

enum LastArrowPress {
    REMOTE_NONE,
    REMOTE_UP,
    REMOTE_DOWN,
    REMOTE_LEFT,
    REMOTE_RIGHT,
    REMOTE_CENTER
};

// TODO: make the readable copy here in Spiram
class ReadableScreen : public Screen, public EinkView {
public:
    ReadableScreen(const std::shared_ptr<RendererStream> &stream, DisplaySystem &display,
                   const std::shared_ptr<Readable> &readable, PreferencesStore &prefs,
                   Telemetry &telemetry, FileSystem &fileSystem,
                   LightingController &lightingController, LocalStore &localStore, Font *font)
        : EinkView(display), stream_(stream),
          renderer_(display, GetTextRectFromPrefs(display, prefs), font), readable_(readable),
          prefs_(prefs), telemetry_(telemetry), fileSystem_(fileSystem),
          lightingController_(lightingController), display_(display), localStore_(localStore) {

        notExist_ = std::make_shared<MsgPopup>(
            this->display_,
            "This content doesn't have that feature.\nPress any button to continue.");

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

        std::function<void()> onMainMenu = [this]() {
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

        std::function<void()> onBookmarkToggle = [this]() {
            inBookmark_ = !inBookmark_;
            writeOutBookmarkUpdate();
        };

        std::function<void()> onReport = [this, &display]() {
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

            auto menuItem = std::make_shared<NewMenuItemSingleLine>(
                "Main Menu", [onMainMenu]() { onMainMenu(); });

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

            auto deleteMenuItem =
                std::make_shared<NewMenuItemSingleLine>("Delete", [this, onMainMenu]() {
                    log_d("Showing delete confirmation!");
                    auto lineMenuScreen =
                        std::make_shared<LineMenuScreen>(this->prefs_, this->display_);
                    lineMenuScreen->resetMenuItems();
                    lineMenuScreen->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Cancel",
                                                                                        [this]() {
                    log_d("Aborting delete!");
                    setCurrentScreen(nullptr);
                    back();
                    }));
                    lineMenuScreen->addMenuItem(
                        std::make_shared<NewMenuItemSingleLine>("Delete: I'm sure!",
                                                                [this, onMainMenu]() {
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

            auto archiveMenuItem = std::make_shared<NewMenuItemSingleLine>("Archive",
                                                                           [this, onMainMenu]() {
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
                bool archived =
                    prefs_.readableArchivedChanged.get(readable_->shortenedUuid.c_str());
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
                auto fontMenu = std::make_shared<ReadingSettingsMenuScreen>(
                    this->display_, "Reading Settings", prefs_);
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
                fontMenu->setBackCallback([this]() { setCurrentScreen(nullptr); });
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
            display, prefs, fileSystem, readable, lightingController, renderer_, telemetry_,
            localStore, stream_);

        popupMenuScreen_->addTab("Popup", popupNavigationFactory);

        popupMenuScreen_->addTab("Context", popupContextFactory);

        popupMenuScreen_->addTab("Style", popupStyleFactory);

        popupMenuScreen_->populateMenuView();

        readableSummaryScreen_ = std::make_shared<ReadableSummaryScreen>(
            prefs, fileSystem, readable, display, renderer_);

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

    ~ReadableScreen() override = default;

    // TODO: Someday
    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void writeOutBookmarkUpdate();
    void updateBookmarkStatus();
    auto checkAtEnd() -> bool;
    auto getScreenCurrentCharOffset() -> uint32_t;

    // Used to retrieve the current spine idx and offset in the corresponding file.
    // See test/CatchTests/TestEPub.cpp
    auto getStreamPlace() -> renderer_defs::StreamPlace {
        return renderer_.getStreamPlace(readable_->place);
    }

    auto getStream() { return stream_; }

protected:
    void setCurrentScreen(const std::shared_ptr<Screen> &screen);

    auto getCurrentScreen() -> std::shared_ptr<Screen> { return currentScreen_; }

    void processCommandDemoMode(RemoteCommand command);
    void processCommandAutoPilot(RemoteCommand command);
    void processCommandNormal(RemoteCommand command);
    void showPopupMenu();
    void hidePopupMenu();

    void drawArrows(bool eraseArrow);

    std::shared_ptr<RendererStream> stream_;
    Renderer renderer_;
    std::shared_ptr<Readable> readable_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    FileSystem &fileSystem_;
    LightingController &lightingController_;

    bool firstLoad_ = true;
    bool inBookmark_ = false;
    bool showingFirstTimeView_ = false;

    std::shared_ptr<Place> newPlace_ = nullptr;
    bool atEnd_ = false;
    bool shownIcons_ = false;
    bool renderPopup_ = false;
    bool renderedPartialSummary_ = false;
    bool autoPilotForceFull_ = false;

    DisplaySystem &display_;

    LocalStore &localStore_;

    std::shared_ptr<NewReadablePopupMenuScreen> popupMenuScreen_;
    std::shared_ptr<ReadableSummaryScreen> readableSummaryScreen_{nullptr};
    std::shared_ptr<ReadablePlacemarkHistoryScreen> readablePlacemarkHistoryScreen_{nullptr};
    std::shared_ptr<ReadableDictionaryScreen> readableDictionaryScreen_{nullptr};

    std::vector<std::shared_ptr<Screen>> hookScreens_;

    // Must reset this to 0 on each page turn.
    unsigned int speedLinesToSkip_ = 0;
    unsigned long nextInterval_ = 0;
    // How many ms we should take to read the page
    int pageSpeed_ = 10000;
    bool autoPilotPause_ = false;

    std::shared_ptr<Screen> currentScreen_{nullptr};

    std::shared_ptr<MsgPopup> notExist_{nullptr};

private:
    void turnPage();
    uint32_t readingStartTime_{};
    LastArrowPress lastArrowPress_ = REMOTE_NONE;

    int numUps_ = 0;
};
