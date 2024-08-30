#include "RootUI.hpp"

#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <filesystem>
#include <nvs_flash.h>

#include "Communications/HtmlCleaner.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "EPub/EPubFile.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/BootLoopDetection.hpp"
#include "Misc/FactoryReset.hpp"
#include "Misc/LookBackStream.hpp"
#include "Misc/MenuStyle.hpp"
#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/SolClock.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/EPubs/LastQuestion.epub.hpp"
#include "Storage/EPubs/PsalmsAndProverbs.epub.hpp"
#include "Storage/LocalStore.hpp"
#include "UI/AboutScreen.hpp"
#include "UI/AlignmentScreen.hpp"
#include "UI/AppDownloadScreen.hpp"
#include "UI/BatteryScreen.hpp"
#include "UI/CalibrationScreen.hpp"
#include "UI/ContentSyncScreen.hpp"
#include "UI/ContrastAdjustScreen.hpp"
#include "UI/CrashReportScreen.hpp"
#include "UI/DebugAboutScreen.hpp"
#include "UI/DemoScreen.hpp"
#include "UI/FirmwareUpdateScreen.hpp"
#include "UI/Fonts/Fonts.hpp"
#include "UI/LayoutTestScreen.hpp"
#include "UI/LegalScreen.hpp"
#include "UI/LightingAdjustScreen.hpp"
#include "UI/LineMenuScreen.hpp"
#include "UI/LineSpacingScreen.hpp"
#include "UI/MainMenuScreen.hpp"
#include "UI/MsgPopup.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/OnboardingContentSyncScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/ReleaseNotesScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/StatsScreen.hpp"
#include "UI/TestAlignmentScreen.hpp"
#include "UI/ToggleMenuScreen.hpp"
#include "UI/TutorialScreen.hpp"
#include "UI/WiFiScanScreen.hpp"
#include "Version.hpp"
#include "config.h"
#include "sindarin-debug.h"

#if CONFIG_USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
#endif

#if CONFIG_DISPLAY_EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#include "UI/VcomAdjustScreen.hpp"
#endif

#if USE_BLE
#include <NimBLEDevice.h>
#endif

#define LAST_READABLE_ID "rui-readable"

// NOLINTNEXTLINE(readability-function-size): It's big, I know
void RootUI::setup(DisplaySystem *display, LocalStore &localStore, FileSystem &fileSystem,
                   LightingController &lightingController) {

    display_ = display;
    auto lastMagReading = power_.getLastMagReading();

    std::unordered_map<std::string, TelemetryVariant> bootData = {
        {"fw", SemanticVersionString()},
        {"sha", GitVersionString()},
        {"bc", prefs_.bootCount.get().value_or(0)},
        {"br", power_.getBootInfo().telemetryStr()},
        {"mag_x", lastMagReading.x},
        {"mag_z", lastMagReading.z},
    };

    auto badBootCount = GetBadBootCount();
    if (badBootCount > 0) {
        bootData["bad_bc"] = badBootCount;
    }

    this->telemetry_.logMetricData(TelemetryField::METRIC_BOOT, bootData);

    MenuStyle styleFamilies;

    auto mainMenu = new MainMenuScreen(power_.battery, *display_, styleFamilies.mainMenu);

    SIZE_OF(*mainMenu);

    auto &readables = localStore.books();
    auto &articles = localStore.articles();
    auto noContent = readables.empty() && articles.empty();
    // If there is no content, add some default content
    if (noContent) {
        log_i("No content found, adding default readables to menu");

        std::shared_ptr<Readable> lastQuestion =
            std::make_shared<Readable>("d7570c42-18a9-4795-a44e-9cee09b9fe3e", "The Last Question",
                                       LASTQUESTION_EPUB, LASTQUESTION_EPUB_SIZE);
        lastQuestion->authors.emplace_back("91ae1429-c6d4-4b8b-88fa-81041d838f92", "Isaac Asimov",
                                           "");
        if (!prefs_.readableArchivedChanged.get(lastQuestion->shortenedUuid.c_str())) {
            readables.push_back(lastQuestion);
        }
        // If there is no content, and we haven't diabled the Bible, add Psalms and Proverbs
        if (!prefs_.noBible.get().value_or(false)) {
            std::shared_ptr<Readable> psalmsAndProverbs = std::make_shared<Readable>(
                "6863fd3c-77a9-468e-93fa-f70d8b79029f", "Psalms and Proverbs",
                PSALMSANDPROVERBS_EPUB, PSALMSANDPROVERBS_EPUB_SIZE);
            psalmsAndProverbs->authors.emplace_back("7632d6ec-7339-4752-8672-a966503e1003",
                                                    "Crossway ESV", "");
            if (!prefs_.readableArchivedChanged.get(lastQuestion->shortenedUuid.c_str())) {
                readables.push_back(psalmsAndProverbs);
            }
        }
    }

    // If prefs_.bookSortCriteria isn't selected, sort the readables by last update and store them
    if (!prefs_.bookSortCriteria.get().has_value()) {
        log_i("No book sort order chosen, sorting by last update and saving readables");
        prefs_.bookSortCriteria.set(SortCriteria::DateModified);
        prefs_.bookSortDirection.set(SortDirection::Descending);
        prefs_.articleSortCriteria.set(SortCriteria::DateModified);
        prefs_.articleSortDirection.set(SortDirection::Descending);
        localStore.saveReadables(readables);
        localStore.saveReadables(articles);
    } else {
        if (static_cast<SortCriteria::Type>(this->prefs_.bookSortCriteria.get().value_or(0)) ==
            SortCriteria::LastRead) {
            // Dynamically sort readables by last read time
            log_i("Dynamically sorting readables by last read time");
            localStore.sortReadables(readables);
        }
        if (static_cast<SortCriteria::Type>(this->prefs_.articleSortCriteria.get().value_or(0)) ==
            SortCriteria::LastRead) {
            // Dynamically sort articles by last read time
            log_i("Dynamically sorting articles by last read time");
            localStore.sortReadables(articles);
        }
    }

    auto booksMenu = new Menu(mainMenu->getMyStyle(), "Books");
    auto articlesMenu = new Menu(mainMenu->getMyStyle(), "Articles");

    // Bible hack (to show off if we wanted to have different "apps" that had different
    // behavior. For now, it's just the same as a regular book.)
    auto isBible = [](const std::shared_ptr<Readable> &r) -> bool {
        return (r->title == "Psalms and Proverbs" || r->title == "New Testament" ||
                r->title == "Old Testament" || r->title == "Torah");
    };
    auto bibleMenu = new Menu(mainMenu->getMyStyle(), "Bible");
    auto it = std::find_if(readables.begin(), readables.end(),
                           [isBible](const std::shared_ptr<Readable> &x) { return isBible(x); });
    auto hasBible = it != readables.end();

    auto createReadableScreen = [this, display, mainMenu, booksMenu, bibleMenu, articlesMenu,
                                 hasBible, &fileSystem, &lightingController, &localStore](
                                    const std::shared_ptr<Readable> &readable) -> ReadableScreen * {
        // TODO: Would be better to not allocate / delete these all the time

        std::shared_ptr<RendererStream> stream;

        if (readable->localOnly && readable->localBytes != nullptr &&
            readable->localBytesSize > 0) {
            // Handle this is an in-memory epub
            auto file =
                SFile(std::make_shared<RamFile>(readable->localBytes, readable->localBytesSize));
            stream = std::make_shared<EPubFile>(file, readable);
        } else {
            auto [path, size] = localStore.readableFilePathForReading(readable);
            if (HasSuffix(path, ".epub")) {
                log_i("Opening %s as epub", path.c_str());
                stream = fileSystem.newEPubRendererStream(path.c_str(), readable);
            } else if (HasSuffix(path, ".txt.gz")) {
                log_i("Opening %s as gzip", path.c_str());
                stream = fileSystem.newGzipRendererStream(path.c_str());
            } else {
                stream = fileSystem.newRendererStream(path.c_str());
            }
        }

        if (stream == nullptr) {
            return nullptr;
        }

        auto *readableScreen =
            new ReadableScreen(stream, *display, readable, this->prefs_, this->telemetry_,
                               fileSystem, lightingController, localStore);

        this->telemetry_.logMetric(TelemetryField::METRIC_READABLE_OPEN, readable->uuid,
                                   readable->telemetryPlacemarkMetrics());

        // When switching to a readable from a menu, we should clear the display
        // to make the whites really white and the blacks really black.
        display->clearDisplay();
        readableScreen->setBackCallback([this, display, stream, readableScreen, readable, booksMenu,
                                         bibleMenu, hasBible, articlesMenu, mainMenu]() {
            bool archived = prefs_.readableArchivedChanged.get(readable->shortenedUuid.c_str());
            auto title =
                StringFormat("%s%s", archived ? "Archived: " : "", readable->title.c_str());
            auto displayedTitle =
                StringFormat("%s%s", !archived ? "Archived: " : "", readable->title.c_str());
            log_d("Changing readable title to %s", title.c_str());
            // Iterate through the menu items and change the title of the item that matches by title
            auto updateMenuTitle = [displayedTitle, title](auto &menu) {
                auto readableMenuItem = menu->getMenuItemByTitle(displayedTitle);
                if (readableMenuItem != -1) {
                    menu->changeItemTitleAt(readableMenuItem, title);
                }
            };

            if (readable->isArticle()) {
                updateMenuTitle(articlesMenu);
            } else {
                updateMenuTitle(booksMenu);
            }
            if (!prefs_.noBible.get().value_or(false) && hasBible) {
                updateMenuTitle(bibleMenu);
            }

            // When leaving a readable to go back to the main menu, do a full clear
            // of the display.
            prefs_.lastReadableId.remove();
            display->clearDisplay();
            mainMenu->setFirstLoad();
            setCurrentScreen(*mainMenu);

            // TODO: Probably not the most kosher practice to delete the `ReadableScreen` from
            // _inside_ ReadableScreen.backCallback. I suppose we could either mark these for
            // deletion on the next loop by adding them to a vector, OR we could rearrange things
            // so that the `backCallback` is copied by the parent screen, and then executed there.
            delete readableScreen;
        });

        return readableScreen;
    };

    auto createTutorialScreen = [this, display, mainMenu](SpiramString text,
                                                          int bit) -> TutorialScreen * {
        auto tutorialScreen = new TutorialScreen(*display, this->prefs_, std::move(text), bit);

        tutorialScreen->setBackCallback([this, display, mainMenu, tutorialScreen]() {
            display->clearDisplay();
            mainMenu->setFirstLoad();
            setCurrentScreen(*mainMenu);
            delete tutorialScreen;
        });

        return tutorialScreen;
    };

    auto currentReadable = prefs_.lastReadableId.get();

    bool hasLastQuestion = false;
    it = std::find_if(readables.begin(), readables.end(), [](const std::shared_ptr<Readable> &x) {
        return (x->title == "The Last Question");
    });
    if (it == readables.end()) {
        log_d("Could not find readable: %s, not enabling CES or EMC modes", "The Last Question");
    } else {
        hasLastQuestion = true;
    }

    // If somehow these were set but we don't have the right readable, unset them.
    if ((!hasLastQuestion) &&
        (prefs_.cesMode.get().value_or(false) || prefs_.emcMode.get().value_or(false))) {
        prefs_.cesMode.remove();
        prefs_.emcMode.remove();
    }

    // If there is firmware available, show the Update Firmware item in the books tab.
    auto makeReleaseNotesMenuItem =
        [this, &localStore, display, mainMenu](const Firmware &firmware) {
        log_i("Found pending firmware update %s", firmware.version.c_str());
        return MenuItem(
            "Upgrade Software",
            StringFormat("From %s to %s", SemanticVersionString(), firmware.version.c_str()),
            [this, display, mainMenu, &localStore, firmware]() {
            auto releaseNotesScreen =
                new ReleaseNotesScreen(*display, this->prefs_, power_, localStore, firmware);
            display->clearDisplay();
            this->setCurrentScreen(*releaseNotesScreen);
            // auto resizeBox = new ResizeBoxScreen(*display, prefs_);
            // this->setCurrentScreen(*resizeBox);
            releaseNotesScreen->setBackCallback([this, display, releaseNotesScreen, mainMenu]() {
                display->clearDisplay();
                mainMenu->setFirstLoad();
                setCurrentScreen(*mainMenu);
                delete releaseNotesScreen;
            });
            });
    };

    if (prefs_.firmwareLoaded.get().has_value()) {
        Firmware firmware;
        auto upgradeAvailable = localStore.getPendingFirmwareUpdate(firmware);
        if (upgradeAvailable) {
            booksMenu->addMenuItem(makeReleaseNotesMenuItem(firmware));
        }
    }

    auto makeContentSyncMenuItem = [this, display, &localStore,
                                    mainMenu](const char *title) -> MenuItem {
        return {title,
                [this, display, &localStore, mainMenu]() {
            // If we're not yet paired, show the app screen instead
            if (PreferencesStore::paired == PairedStatus::Unpaired) {
                auto appDownloadScreen = new AppDownloadScreen(*display);
                this->setCurrentScreen(*appDownloadScreen);
                appDownloadScreen->setBackCallback([this, appDownloadScreen, mainMenu, display]() {
                    display->clearDisplay();
                    mainMenu->setFirstLoad();
                    setCurrentScreen(*mainMenu);
                    delete appDownloadScreen;
                });
                return;
            }

            auto contentSyncScreen =
                new ContentSyncScreen(*display, localStore, this->prefs_, this->telemetry_);
            display->setFont(mainMenu->getMyStyle().navigationFont);
            this->setCurrentScreen(*contentSyncScreen);
            contentSyncScreen->setBackCallback([this, contentSyncScreen, mainMenu, display]() {
                display->clearDisplay();
                mainMenu->setFirstLoad();
                setCurrentScreen(*mainMenu);
                mainMenu->render(*display);
                delete contentSyncScreen;
            });
            contentSyncScreen->begin();
                }};
    };

    // If there are no books, show the Sync Content item in the books tab.
    if (noContent) {
        booksMenu->addMenuItem(makeContentSyncMenuItem("Sync Content"));
    }

    SHOW_HEAP();

    for (const auto &r : readables) {
        bool archived = prefs_.readableArchivedChanged.get(r->shortenedUuid.c_str());
        auto title = StringFormat("%s%s", archived ? "Archived: " : "", r->title.c_str());
        // Bible books go in their own menu / app
        if (!prefs_.noBible.get().value_or(false) && isBible(r)) {
            continue;
        }

        booksMenu->addMenuItem(MenuItem(StringFormat("%s", title.c_str()),
                                        StringFormat("%s", r->authorString().c_str()),
                                        [this, r, display, mainMenu, createReadableScreen]() {
            auto readableScreen = createReadableScreen(r);
            if (readableScreen != nullptr) {
                this->setCurrentScreen(*readableScreen);
            } else {
                auto *errorPopup = new MsgPopup(*display, itemErrorMsg_);
                this->setCurrentScreen(*errorPopup);
                errorPopup->setBackCallback([this, errorPopup, mainMenu]() {
                    mainMenu->setFirstLoad();
                    display_->clearDisplay();
                    setCurrentScreen(*mainMenu);
                    delete errorPopup;
                });
            }
        }));
    }

    SHOW_HEAP();

    if (!(prefs_.tutorialsDeleted.get().value_or(0) & TUTORIAL_SCREEN_BOOKS)) {
        booksMenu->addMenuItem(MenuItem("\ue000 Books Tutorial", "",
                                        [this, createTutorialScreen]() {
            SpiramString message = SpiramString(
                "Welcome to the Books menu. This tab "
                "shows you the books currently synced to your Sol Reader. To add more books or "
                "manage the books on your device, use the Sol mobile app.\n\n"
                "Did you know "
                "you can change the position, width, and height of the page in the Sol Reader "
                "display? "
                "Simply go to Settings -> Reading -> Resize Page."
                "\n\n\nPress "
                "\ue000 (center) to continue.");
            auto tutorialScreen = createTutorialScreen(message, TUTORIAL_SCREEN_BOOKS);
            this->setCurrentScreen(*tutorialScreen);
        }));
    }

    if (!prefs_.noBooks.get().value_or(false)) {
        mainMenu->addSubMenu(booksMenu);
    }

    log_i("Loading %zu Pocket articles", articles.size());

    if (prefs_.firmwareLoaded.get().has_value()) {
        Firmware firmware;
        auto upgradeAvailable = localStore.getPendingFirmwareUpdate(firmware);
        if (upgradeAvailable) {
            articlesMenu->addMenuItem(makeReleaseNotesMenuItem(firmware));
        }
    }

    // If there are no articles, show the Sync Content item in the articles tab.
    if (articles.empty()) {
        articlesMenu->addMenuItem(makeContentSyncMenuItem("Sync Content"));
    }

    for (const auto &r : articles) {
        bool archived = prefs_.readableArchivedChanged.get(r->shortenedUuid.c_str());
        auto title = StringFormat("%s%s", archived ? "Archived: " : "", r->title.c_str());

        articlesMenu->addMenuItem(MenuItem(title,
                                           [this, display, mainMenu, r, createReadableScreen]() {
            display->clearDisplay();
            auto readableScreen = createReadableScreen(r);
            if (readableScreen != nullptr) {
                setCurrentScreen(*readableScreen);
            } else {
                auto *errorPopup = new MsgPopup(*display, itemErrorMsg_);
                this->setCurrentScreen(*errorPopup);
                errorPopup->setBackCallback([this, errorPopup, mainMenu]() {
                    mainMenu->setFirstLoad();
                    display_->clearDisplay();
                    setCurrentScreen(*mainMenu);
                    delete errorPopup;
                });
            }
        }));
    }

    SHOW_HEAP();

    if (!(prefs_.tutorialsDeleted.get().value_or(0) & TUTORIAL_SCREEN_ARTICLES)) {
        articlesMenu->addMenuItem(MenuItem("\ue000 Articles Tutorial",
                                           [this, createTutorialScreen]() {
            SpiramString message = SpiRamStringFormat(
                "Welcome to the Articles menu. This tab "
                "shows you the articles currently synced to your Sol Reader. To add more articles, "
                "simply connect your Pocket account in the Sol mobile app or forward newsletters "
                "from your inbox to add@solreader.com.\n\n"
                "Did you know "
                "you can change the sort order of your books and articles? "
                "Simply go to Settings -> Content -> Sort Order."
                "\n\n\nPress \ue000 (center) to "
                "continue.\n");
            auto tutorialScreen = createTutorialScreen(message, TUTORIAL_SCREEN_ARTICLES);
            this->setCurrentScreen(*tutorialScreen);
        }));
    }

    if (!prefs_.noArticles.get().value_or(false)) {
        mainMenu->addSubMenu(articlesMenu);
    }

    if (!prefs_.noBible.get().value_or(false) && hasBible) {
        for (const auto &r : readables) {
            if (isBible(r)) {
                bool archived = prefs_.readableArchivedChanged.get(r->shortenedUuid.c_str());
                auto title = StringFormat("%s%s", archived ? "Archived: " : "", r->title.c_str());
                bibleMenu->addMenuItem(
                    MenuItem(title, [this, r, createReadableScreen, display, mainMenu]() {
                        auto readableScreen = createReadableScreen(r);
                        if (readableScreen != nullptr) {
                            this->setCurrentScreen(*readableScreen);
                        } else {
                            auto *errorPopup = new MsgPopup(*display, itemErrorMsg_);
                            this->setCurrentScreen(*errorPopup);
                            errorPopup->setBackCallback([this, errorPopup, mainMenu]() {
                                mainMenu->setFirstLoad();
                                display_->clearDisplay();
                                setCurrentScreen(*mainMenu);
                                delete errorPopup;
                            });
                        }
                    }));
            }
        }

        if (!(prefs_.tutorialsDeleted.get().value_or(0) & TUTORIAL_SCREEN_BIBLE)) {
            bibleMenu->addMenuItem(MenuItem("\ue000 Why have a Bible app?",
                                            [this, createTutorialScreen]() {
                SpiramString message = SpiRamStringFormat(
                    "The Gutenberg Bible "
                    "was the earliest major book printed using mass-produced movable "
                    "metal type in Europe. It marked the start of the Gutenberg Revolution "
                    "and the age of printed books in the West. So we thought it would be "
                    "appropriate to include the Bible as one of the first books on the Sol "
                    "Reader.\n\n"
                    "Crossway Publishing has generously provided the English "
                    "Standard Version"
                    " of the Bible to you free of charge (Copyright 2001, Crossway Publishing).\n\n"
                    " Not your thing? No worries! You can toggle this app off "
                    "in the Content Settings.\n\n"
                    " Look for more scripture and meditation Sol Reader apps down the road!\n\n\n"
                    " Press \ue000 (center) to continue.");
                auto tutorialScreen = createTutorialScreen(message, TUTORIAL_SCREEN_BIBLE);
                this->setCurrentScreen(*tutorialScreen);
            }));
        }
        mainMenu->addSubMenu(bibleMenu);
    }

    auto settingsMenu = new Menu(styleFamilies.settingsMenu, "Settings");

    auto readingSettingsMenu =
        new MenuScreen(power_.battery, *display, styleFamilies.settingsMenu, "Reading");

    readingSettingsMenu->setBackCallback([this, mainMenu]() {
        this->display_->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
    });

    settingsMenu->addMenuItem(MenuItem("Reading", [this, readingSettingsMenu]() {
        log_d("Loading Reading Settings Menu");
        readingSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*readingSettingsMenu);
    }));

    readingSettingsMenu->addMenuItem(MenuItem("Text Size", [this, display, readingSettingsMenu]() {
        auto fontMenu = new MenuScreen(power_.battery, *display, readingSettingsMenu->getMyStyle(),
                                       "Font Settings", "Reading Settings");
        fontMenu->setExtraText("Example text size");
        fontMenu->addMenuItem(MenuItem("Small", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.solSansSmall);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_SOL_SMALL);
            this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE, {{"text_size", "small"}});
            fontMenu->setNeedsRerender();
        }));
        fontMenu->addMenuItem(MenuItem("Medium", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.solSansMedium);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_SOL_MEDIUM);
            this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE, {{"text_size", "medium"}});
            fontMenu->setNeedsRerender();
        }));
        fontMenu->addMenuItem(MenuItem("Large", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.solSansLarge);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_SOL_LARGE);
            this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE, {{"text_size", "large"}});
            fontMenu->setNeedsRerender();
        }));
        int menuIndex = 0;
        if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_SMALL) {
            menuIndex = 0;
        } else if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_MEDIUM) {
            menuIndex = 1;
        } else if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_LARGE) {
            menuIndex = 2;
        }
        fontMenu->setCurrSelection(menuIndex);
        setCurrentScreen(*fontMenu);
        fontMenu->setBackCallback([this, fontMenu, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete fontMenu;
        });
    }));

#if 0
    if (prefs_.fontsLibrary.get().value_or(false)) {
        readingSettingsMenu->addMenuItem(MenuItem("Font Type (Experimental)",
                                                  [this, display, readingSettingsMenu]() {
            auto fontTypeMenu =
                new MenuScreen(power_.battery, *display, readingSettingsMenu->getMyStyle(),
                               "Font Type Settings", "Reading Settings");
            for (int i = 0; i < fontsList.size(); i++) {
                fontTypeMenu->addMenuItem(MenuItem(fontsList[i].caption, [this, fontTypeMenu, i]() {
                    mainFont.load((MemoryPtr)fontsList[i].content, fontsList[i].length);
                    prefs_.fontType.set(i);
                    this->display_->clearDisplay();
                    fontTypeMenu->setNeedsRerender();
                }));
            }
            setCurrentScreen(*fontTypeMenu);
            fontTypeMenu->setBackCallback([this, fontTypeMenu, readingSettingsMenu]() {
                readingSettingsMenu->setFirstLoad();
                setCurrentScreen(*readingSettingsMenu);
                delete fontTypeMenu;
            });
        }));
    }
#endif

    readingSettingsMenu->addMenuItem(MenuItem("Brightness", [this, display, readingSettingsMenu,
                                                             &lightingController]() {
        auto lightingAdjustScreen =
            new LightingAdjustScreen(*display, this->prefs_, lightingController, this->telemetry_);
        this->display_->clearDisplay();
        setCurrentScreen(*lightingAdjustScreen);
        lightingAdjustScreen->setBackCallback([this, lightingAdjustScreen, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete lightingAdjustScreen;
        });
    }));

#if CONFIG_DISPLAY_EINKET013TT1
    readingSettingsMenu->addMenuItem(MenuItem("Contrast", [this, display, readingSettingsMenu,
                                                           &lightingController]() {
        auto contrastAdjustScreen =
            new ContrastAdjustScreen(*display, this->prefs_, lightingController, this->telemetry_);
        this->display_->clearDisplay();
        setCurrentScreen(*contrastAdjustScreen);
        contrastAdjustScreen->setBackCallback([this, contrastAdjustScreen, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete contrastAdjustScreen;
        });
    }));
#endif

    readingSettingsMenu->addMenuItem(MenuItem("Move Page in Display",
                                              [this, readingSettingsMenu, display]() {
        log_d("Loading adjust offset");
        auto offsetAdjust = new OffsetAdjustScreen(*display, prefs_, this->telemetry_);
        this->setCurrentScreen(*offsetAdjust);
        log_d("setting up callback");
        offsetAdjust->setBackCallback([this, offsetAdjust, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete offsetAdjust;
        });
        log_d("set up callback");
    }));

    readingSettingsMenu->addMenuItem(MenuItem("Resize Page",
                                              [this, readingSettingsMenu, display]() {
        log_d("Loading resize box");
        auto resizeBox = new ResizeBoxScreen(*display, prefs_, this->telemetry_);
        this->setCurrentScreen(*resizeBox);
        log_d("setting up callback");
        resizeBox->setBackCallback([this, resizeBox, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete resizeBox;
        });
        log_d("set up callback");
    }));

    readingSettingsMenu->addMenuItem(MenuItem("Line Spacing",
                                              [this, readingSettingsMenu, display]() {
        auto lineSpacingScreen = new LineSpacingScreen(*display, prefs_, this->telemetry_);
        this->setCurrentScreen(*lineSpacingScreen);
        lineSpacingScreen->setBackCallback([this, lineSpacingScreen, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete lineSpacingScreen;
        });
    }));

    readingSettingsMenu->addMenuItem(MenuItem("Inactivity Timer",
                                              [this, readingSettingsMenu, display]() {
        auto inactivityMenu =
            new MenuScreen(power_.battery, *display, readingSettingsMenu->getMyStyle(),
                           "Inactivity Timer", "Reading Settings");
        auto updateExtraText = [this, inactivityMenu]() {
            this->display_->style.setFontFamily(this->display_->style.solSansSmall);
            std::string message = std::string(SpiRamStringFormat(
                "Screen will sleep after %d minutes of inactivity.",
                prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES)));
            inactivityMenu->setExtraText(message);
        };
        auto addInactivityMenuItem =
            [this, inactivityMenu, updateExtraText](const std::string &label, int minutes,
                                                    const FontFamily fontFamily) {
            inactivityMenu->addMenuItem(
                MenuItem(label, [this, inactivityMenu, updateExtraText, minutes, fontFamily]() {
                    this->display_->style.setFontFamily(fontFamily);
                    this->display_->clearDisplay();
                    prefs_.inactivityMinutes.set(minutes);
                    updateExtraText();
                    inactivityMenu->requireRestart();
                    this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                   {{"inactivity_minutes", minutes}});
                    inactivityMenu->setNeedsRerender();
                }));
        };

        addInactivityMenuItem("5 minutes", 5, this->display_->style.solSansSmall);
        addInactivityMenuItem("10 minutes", 10, this->display_->style.solSansMedium);
        addInactivityMenuItem("15 minutes", 15, this->display_->style.solSansLarge);
        addInactivityMenuItem("20 minutes", 20, this->display_->style.solSansLarge);
        addInactivityMenuItem("30 minutes", 30, this->display_->style.solSansLarge);
        int menuIndex = 0;
        if (prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES) == 5) {
            menuIndex = 0;
        } else if (prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES) == 10) {
            menuIndex = 1;
        } else if (prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES) == 15) {
            menuIndex = 2;
        } else if (prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES) == 20) {
            menuIndex = 3;
        } else if (prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES) == 30) {
            menuIndex = 4;
        }
        updateExtraText();
        inactivityMenu->setCurrSelection(menuIndex);
        setCurrentScreen(*inactivityMenu);
        inactivityMenu->setBackCallback([this, inactivityMenu, readingSettingsMenu, display]() {
            if (inactivityMenu->getRequireRestart()) {
                PreferencesStore::settingsOnRestart = true;
                showTemporaryMessage(*display, settingsRestartMsg_, 1500, false);
                this->power_.reboot();
            } else {
                readingSettingsMenu->setFirstLoad();
                setCurrentScreen(*readingSettingsMenu);
                delete inactivityMenu;
            }
        });
    }));

    readingSettingsMenu->addMenuItem(MenuItem("Statistics", [this, readingSettingsMenu, display]() {
        auto statsScreen = new StatsScreen(*display, prefs_);
        this->setCurrentScreen(*statsScreen);
        statsScreen->setBackCallback([this, statsScreen, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete statsScreen;
        });
    }));

    auto contentSettingsMenu =
        new ToggleMenuScreen(power_.battery, *display_, styleFamilies.settingsMenu, "Content");

    settingsMenu->addMenuItem(MenuItem("Content", [this, contentSettingsMenu]() {
        log_d("Loading Content Settings Menu");
        contentSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*contentSettingsMenu);
    }));

    contentSettingsMenu->setBackCallback([this, mainMenu, display, contentSettingsMenu]() {
        // Ideally we'd move this into the ToggleClass so it's not duplicated with
        // experimentalSettingsMenu below, but that would require teh ToggleMenuScreen
        // to have the power intead of just the battery and also the prefs. Big deal or not?
        if (contentSettingsMenu->getRequireRestart()) {
            // Start on the settings menu when we boot so it feels like we just
            // went back to it!
            PreferencesStore::settingsOnRestart = true;
            contentSettingsMenu->showTemporaryMessage(*display, settingsRestartMsg_, 1500, false);
            this->power_.reboot();
        } else {
            this->display_->clearDisplay();
            mainMenu->setFirstLoad();
            setCurrentScreen(*mainMenu);
        }
    });

    contentSettingsMenu->addMenuItem(MenuItem("Books", [this, contentSettingsMenu]() {
        this->prefs_.noBooks.set(!this->prefs_.noBooks.get().value_or(false));
        contentSettingsMenu->requireRestart();
    }));

    if (prefs_.noBooks.get().value_or(false)) {
        contentSettingsMenu->setToggleLastItem(false);
    } else {
        contentSettingsMenu->setToggleLastItem(true);
    }

// At some point, we may split out News (emails) and Articles (pocket), but for now
// We'll keep it the same.
#if 0
    contentSettingsMenu->addMenuItem(MenuItem("News", [this, display, contentSettingsMenu]() {
        this->prefs_.showNews.set(!this->prefs_.showNews.get().value_or(false));
    }));

    if (prefs_.showNews.get().value_or(false)) {
        contentSettingsMenu->setToggleLastItem(true);
    } else {
        contentSettingsMenu->setToggleLastItem(false);
    }
#endif

    contentSettingsMenu->addMenuItem(MenuItem("Articles", [this, contentSettingsMenu]() {
        this->prefs_.noArticles.set(!this->prefs_.noArticles.get().value_or(false));
        contentSettingsMenu->requireRestart();
    }));

    if (prefs_.noArticles.get().value_or(false)) {
        contentSettingsMenu->setToggleLastItem(false);
    } else {
        contentSettingsMenu->setToggleLastItem(true);
    }

    contentSettingsMenu->addMenuItem(MenuItem("Bible", [this, contentSettingsMenu]() {
        this->prefs_.noBible.set(!this->prefs_.noBible.get().value_or(false));
        contentSettingsMenu->requireRestart();
    }));

    if (prefs_.noBible.get().value_or(false)) {
        contentSettingsMenu->setToggleLastItem(false);
    } else {
        contentSettingsMenu->setToggleLastItem(true);
    }

    contentSettingsMenu->addMenuItem(MenuItem("Sync content on close", [this]() {
        this->prefs_.noDeliverOnClose.set(!this->prefs_.noDeliverOnClose.get().value_or(false));
    }));

    if (prefs_.noDeliverOnClose.get().value_or(false)) {
        contentSettingsMenu->setToggleLastItem(false);
    } else {
        contentSettingsMenu->setToggleLastItem(true);
    }

    contentSettingsMenu->addMenuItem(MenuItem("Morning content sync", [this]() {
        this->prefs_.noMorningDelivery.set(!this->prefs_.noMorningDelivery.get().value_or(false));
    }));

    if (prefs_.noMorningDelivery.get().value_or(false)) {
        contentSettingsMenu->setToggleLastItem(false);
    } else {
        contentSettingsMenu->setToggleLastItem(true);
    }

    contentSettingsMenu->addMenuItem(MenuItem("Evening content sync", [this]() {
        this->prefs_.eveningDelivery.set(!this->prefs_.eveningDelivery.get().value_or(false));
    }));

    if (prefs_.eveningDelivery.get().value_or(false)) {
        contentSettingsMenu->setToggleLastItem(true);
    } else {
        contentSettingsMenu->setToggleLastItem(false);
    }

    for (auto modelType : {ReadablesModelType::Book, ReadablesModelType::Article}) {
        auto sortOrderMenuItem = MenuItem(
            modelType == ReadablesModelType::Book ? "Book sort order" : "Article sort order",
            [this, &localStore, display, contentSettingsMenu, modelType]() {
            auto sortMenu = new MenuScreen(
                power_.battery, *display, contentSettingsMenu->getMyStyle(),
                modelType == ReadablesModelType::Book ? "Book Sort Order" : "Article Sort Order",
                "Content Settings");
            sortMenu->addMenuItem(MenuItem("Alphabetical", [this, &localStore, display,
                                                            contentSettingsMenu, modelType]() {
                if (modelType == ReadablesModelType::Book) {
                    this->prefs_.bookSortCriteria.set(static_cast<int>(SortCriteria::Alphabetical));
                    this->prefs_.bookSortDirection.set(static_cast<int>(SortDirection::Ascending));
                    localStore.saveReadables(localStore.books());
                } else if (modelType == ReadablesModelType::Article) {
                    this->prefs_.articleSortCriteria.set(
                        static_cast<int>(SortCriteria::Alphabetical));
                    this->prefs_.articleSortDirection.set(
                        static_cast<int>(SortDirection::Ascending));
                    localStore.saveReadables(localStore.articles());
                }
                contentSettingsMenu->showTemporaryMessage(*display, settingsRestartMsg_, 500,
                                                          false);
                this->power_.reboot();
            }));
            sortMenu->addMenuItem(
                MenuItem("Date added, most recent",
                         [this, &localStore, display, contentSettingsMenu, modelType]() {
                if (modelType == ReadablesModelType::Book) {
                    this->prefs_.bookSortCriteria.set(static_cast<int>(SortCriteria::DateModified));
                    this->prefs_.bookSortDirection.set(static_cast<int>(SortDirection::Descending));
                    localStore.saveReadables(localStore.books());
                } else if (modelType == ReadablesModelType::Article) {
                    this->prefs_.articleSortCriteria.set(
                        static_cast<int>(SortCriteria::DateModified));
                    this->prefs_.articleSortDirection.set(
                        static_cast<int>(SortDirection::Descending));
                    localStore.saveReadables(localStore.articles());
                }
                contentSettingsMenu->showTemporaryMessage(*display, settingsRestartMsg_, 500,
                                                          false);
                this->power_.reboot();
                }));
            sortMenu->addMenuItem(
                MenuItem("Date added, least recent",
                         [this, &localStore, display, contentSettingsMenu, modelType]() {
                if (modelType == ReadablesModelType::Book) {
                    this->prefs_.bookSortCriteria.set(static_cast<int>(SortCriteria::DateModified));
                    this->prefs_.bookSortDirection.set(static_cast<int>(SortDirection::Ascending));
                    localStore.saveReadables(localStore.books());
                } else if (modelType == ReadablesModelType::Article) {
                    this->prefs_.articleSortCriteria.set(
                        static_cast<int>(SortCriteria::DateModified));
                    this->prefs_.articleSortDirection.set(
                        static_cast<int>(SortDirection::Ascending));
                    localStore.saveReadables(localStore.articles());
                }
                contentSettingsMenu->showTemporaryMessage(*display, settingsRestartMsg_, 500,
                                                          false);
                this->power_.reboot();
                }));
            sortMenu->addMenuItem(
                MenuItem("Last read, most recent",
                         [this, &localStore, display, contentSettingsMenu, modelType]() {
                if (modelType == ReadablesModelType::Book) {
                    this->prefs_.bookSortCriteria.set(static_cast<int>(SortCriteria::LastRead));
                    this->prefs_.bookSortDirection.set(static_cast<int>(SortDirection::Descending));
                    localStore.saveReadables(localStore.books());
                } else if (modelType == ReadablesModelType::Article) {
                    this->prefs_.articleSortCriteria.set(static_cast<int>(SortCriteria::LastRead));
                    this->prefs_.articleSortDirection.set(
                        static_cast<int>(SortDirection::Descending));
                    localStore.saveReadables(localStore.articles());
                }
                contentSettingsMenu->showTemporaryMessage(*display, settingsRestartMsg_, 500,
                                                          false);
                this->power_.reboot();
                }));
            sortMenu->addMenuItem(
                MenuItem("Last read, least recent",
                         [this, &localStore, display, contentSettingsMenu, modelType]() {
                if (modelType == ReadablesModelType::Book) {
                    this->prefs_.bookSortCriteria.set(static_cast<int>(SortCriteria::LastRead));
                    this->prefs_.bookSortDirection.set(static_cast<int>(SortDirection::Ascending));
                    localStore.saveReadables(localStore.books());
                } else if (modelType == ReadablesModelType::Article) {
                    this->prefs_.articleSortCriteria.set(static_cast<int>(SortCriteria::LastRead));
                    this->prefs_.articleSortDirection.set(
                        static_cast<int>(SortDirection::Ascending));
                    localStore.saveReadables(localStore.articles());
                }
                contentSettingsMenu->showTemporaryMessage(*display, settingsRestartMsg_, 500,
                                                          false);
                this->power_.reboot();
                }));
            sortMenu->addMenuItem(MenuItem("Random", [this, &localStore, display,
                                                      contentSettingsMenu, modelType]() {
                if (modelType == ReadablesModelType::Book) {
                    this->prefs_.bookSortCriteria.set(static_cast<int>(SortCriteria::Random));
                    this->prefs_.bookSortDirection.set(static_cast<int>(SortDirection::Ascending));
                    localStore.saveReadables(localStore.books());
                } else if (modelType == ReadablesModelType::Article) {
                    this->prefs_.articleSortCriteria.set(static_cast<int>(SortCriteria::Random));
                    this->prefs_.articleSortDirection.set(
                        static_cast<int>(SortDirection::Ascending));
                    localStore.saveReadables(localStore.articles());
                }
                contentSettingsMenu->showTemporaryMessage(*display, settingsRestartMsg_, 500,
                                                          false);
                this->power_.reboot();
            }));
            int menuIndex = 0;
            auto sortCriteria = modelType == ReadablesModelType::Book
                                    ? this->prefs_.bookSortCriteria
                                    : this->prefs_.articleSortCriteria;
            auto sortDirection = modelType == ReadablesModelType::Book
                                     ? this->prefs_.bookSortDirection
                                     : this->prefs_.articleSortDirection;

            switch (static_cast<SortCriteria::Type>(sortCriteria.get().value_or(0))) {
            case SortCriteria::Alphabetical:
                menuIndex = 0;
                break;
            case SortCriteria::DateModified:
                if (static_cast<SortDirection::Type>(sortDirection.get().value_or(0)) ==
                    SortDirection::Descending) {
                    menuIndex = 1;
                } else {
                    menuIndex = 2;
                }
                break;
            case SortCriteria::LastRead:
                if (static_cast<SortDirection::Type>(sortDirection.get().value_or(0)) ==
                    SortDirection::Descending) {
                    menuIndex = 3;
                } else {
                    menuIndex = 4;
                }
                break;
            case SortCriteria::Random:
                menuIndex = 5;
                break;
            }
            sortMenu->setCurrSelection(menuIndex);
            setCurrentScreen(*sortMenu);
            sortMenu->setBackCallback([this, sortMenu, contentSettingsMenu]() {
                contentSettingsMenu->setFirstLoad();
                setCurrentScreen(*contentSettingsMenu);
                delete sortMenu;
            });
            });
        if (modelType == ReadablesModelType::Book) {
            sortOrderMenuItem.topPadding = 10;
        }
        contentSettingsMenu->addMenuItem(sortOrderMenuItem);

        // Hide the toggle for the sort order menu
        contentSettingsMenu->setToggleLastItem(false, false);
    }

    contentSettingsMenu->addMenuItem(makeContentSyncMenuItem("Sync content now"));

    // Hide the bubble for the last item, Sync content now
    contentSettingsMenu->setToggleLastItem(false, false);

    auto systemSettingsMenu =
        new MenuScreen(power_.battery, *display, styleFamilies.settingsMenu, "System");

    systemSettingsMenu->setBackCallback([this, mainMenu]() {
        this->display_->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
    });

    settingsMenu->addMenuItem(MenuItem("System", [this, systemSettingsMenu]() {
        log_d("Loading System Settings Menu");
        systemSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*systemSettingsMenu);
    }));

    systemSettingsMenu->addMenuItem(MenuItem("About", [this, systemSettingsMenu, display]() {
        auto aboutScreen = new AboutScreen(*display, prefs_, power_);
        this->setCurrentScreen(*aboutScreen);
        aboutScreen->setBackCallback([this, aboutScreen, systemSettingsMenu]() {
            systemSettingsMenu->setFirstLoad();
            display_->clearDisplay();
            setCurrentScreen(*systemSettingsMenu);
            delete aboutScreen;
        });
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Legal & Regulatory",
                                             [this, systemSettingsMenu, display]() {
        auto legalScreen = new LegalScreen(*display, prefs_, power_);
        this->setCurrentScreen(*legalScreen);
        legalScreen->setBackCallback([this, legalScreen, systemSettingsMenu]() {
            systemSettingsMenu->setFirstLoad();
            display_->clearDisplay();
            setCurrentScreen(*systemSettingsMenu);
            delete legalScreen;
        });
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Firmware Update",
                                             [this, systemSettingsMenu, display]() {
        // If we don't have WiFi configured, show the QR code instead
        if (!WiFi::getInstance().hasWiFiCredentials()) {
            auto appDownloadScreen = new AppDownloadScreen(*display);
            this->setCurrentScreen(*appDownloadScreen);
            appDownloadScreen->setBackCallback(
                [this, appDownloadScreen, systemSettingsMenu, display]() {
                display->clearDisplay();
                systemSettingsMenu->setFirstLoad();
                setCurrentScreen(*systemSettingsMenu);
                delete appDownloadScreen;
            });
            return;
        }

        auto firmwareUpdateScreen = new FirmwareUpdateScreen(*display, telemetry_, prefs_);
        this->setCurrentScreen(*firmwareUpdateScreen);
        firmwareUpdateScreen->setBackCallback(
            [this, firmwareUpdateScreen, systemSettingsMenu, display]() {
            systemSettingsMenu->setFirstLoad();
            setCurrentScreen(*systemSettingsMenu);
            systemSettingsMenu->render(*display);
            delete firmwareUpdateScreen;
        });
        firmwareUpdateScreen->begin();
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Manifesto", [this, systemSettingsMenu, display]() {
        auto demoScreen = new DemoScreen(*display, prefs_);
        this->setCurrentScreen(*demoScreen);
        demoScreen->setBackCallback([this, demoScreen, systemSettingsMenu, display]() {
            systemSettingsMenu->setFirstLoad();
            display->clearDisplay();
            setCurrentScreen(*systemSettingsMenu);
            systemSettingsMenu->render(*this->display_);
            delete demoScreen;
        });
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Reset Statistics",
                                             [this, systemSettingsMenu, display]() {
        prefs_.wordCount.remove();
        prefs_.totalReadingTimeSeconds.remove();
        systemSettingsMenu->showTemporaryMessage(*display, "Statistics reset.");
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Clear All Content and Restart",
                                             [this, systemSettingsMenu, display, &localStore]() {
        localStore.deleteReadables(false);
        localStore.deleteArticles(false);
        systemSettingsMenu->showTemporaryMessage(*display, "Content cleared.", 2000, false);
        power_.reboot();
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Clear WiFi Settings",
                                             [systemSettingsMenu, display]() {
        WiFi::getInstance().clearWiFiCredentials();
        log_w("WiFi settings cleared.");
        systemSettingsMenu->showTemporaryMessage(*display, "WiFi settings cleared.", 2000, true);
    }));

    systemSettingsMenu->addMenuItem(MenuItem("WiFi Scan", [this, systemSettingsMenu, display]() {
        auto wiFiScanScreen = new WiFiScanScreen(*display, WiFi::getInstance());
        wiFiScanScreen->begin();
        this->setCurrentScreen(*wiFiScanScreen);
        wiFiScanScreen->setBackCallback([this, wiFiScanScreen, systemSettingsMenu, display]() {
            systemSettingsMenu->setFirstLoad();
            display->clearDisplay();
            setCurrentScreen(*systemSettingsMenu);
            systemSettingsMenu->render(*this->display_);
            delete wiFiScanScreen;
        });
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Factory Defaults",
                                             [this, systemSettingsMenu, display, &fileSystem]() {
        SoftFactoryReset(fileSystem);
        log_w("NVS and FS cleared. Rebooting...");
        systemSettingsMenu->showTemporaryMessage(*display, "Resetting Device! Rebooting...");
        power_.reboot();
    }));

    auto experimentalSettingsMenu =
        new ToggleMenuScreen(power_.battery, *display_, styleFamilies.settingsMenu, "Experimental");

    experimentalSettingsMenu->setBackCallback(
        [this, display, mainMenu, experimentalSettingsMenu]() {
        if (experimentalSettingsMenu->getRequireRestart()) {
            // Start on the settings menu when we boot so it feels like we just
            // went back to it!
            PreferencesStore::settingsOnRestart = true;
            experimentalSettingsMenu->showTemporaryMessage(*display, settingsRestartMsg_, 1500,
                                                           false);
            this->power_.reboot();
        } else {
            this->display_->clearDisplay();
            mainMenu->setFirstLoad();
            setCurrentScreen(*mainMenu);
        }
    });

    settingsMenu->addMenuItem(MenuItem("Experimental", [this, experimentalSettingsMenu]() {
        log_d("Loading Content Settings Menu");
        experimentalSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*experimentalSettingsMenu);
    }));

    experimentalSettingsMenu->addMenuItem(MenuItem("Dark Mode", [this, experimentalSettingsMenu]() {
        this->prefs_.darkMode.set(!this->prefs_.darkMode.get().value_or(false));
        // Reserved for future use? Potentially lighting up and down?
        this->display_->flipInversion();
        // When we invert, we should do a full clear of the display to make sure
        // the blacks are really gone.
        this->display_->clearDisplay();
        prefs_.invertSetting.set(this->display_->getInversion());
        experimentalSettingsMenu->setNeedsRerender();
        experimentalSettingsMenu->setFirstLoad();
    }));

    if (prefs_.darkMode.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(true);
    } else {
        experimentalSettingsMenu->setToggleLastItem(false);
    }

#if 0
    experimentalSettingsMenu->addMenuItem(MenuItem("Fonts Library",
                                                   [this, experimentalSettingsMenu]() {
        this->prefs_.fontsLibrary.set(!this->prefs_.fontsLibrary.get().value_or(false));
        experimentalSettingsMenu->requireRestart();
    }));

    if (prefs_.fontsLibrary.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(true);
    } else {
        experimentalSettingsMenu->setToggleLastItem(false);
    }
#endif

    experimentalSettingsMenu->addMenuItem(MenuItem("Autopilot", [this]() {
        this->prefs_.autoPilot.set(!this->prefs_.autoPilot.get().value_or(false));
        if (this->prefs_.autoPilot.get().value_or(false)) {
            prefs_.autoPilotSpeed.set(10000);
        } else {
            prefs_.autoPilotSpeed.remove();
        }
    }));

    if (prefs_.autoPilot.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(true);
    } else {
        experimentalSettingsMenu->setToggleLastItem(false);
    }

    experimentalSettingsMenu->addMenuItem(MenuItem("Show summary", [this]() {
        this->prefs_.noShowPartialChapterSummary.set(
            !this->prefs_.noShowPartialChapterSummary.get().value_or(false));
    }));

    if (prefs_.noShowPartialChapterSummary.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(false);
    } else {
        experimentalSettingsMenu->setToggleLastItem(true);
    }

    experimentalSettingsMenu->addMenuItem(MenuItem("Bookmark Right Select", [this]() {
        this->prefs_.fastBookmarkRight.set(!this->prefs_.fastBookmarkRight.get().value_or(false));
    }));

    if (prefs_.fastBookmarkRight.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(true);
    } else {
        experimentalSettingsMenu->setToggleLastItem(false);
    }

    experimentalSettingsMenu->addMenuItem(MenuItem("Bookmark Center Select", [this]() {
        this->prefs_.fastBookmarkCenter.set(!this->prefs_.fastBookmarkCenter.get().value_or(false));
    }));

    if (prefs_.fastBookmarkCenter.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(true);
    } else {
        experimentalSettingsMenu->setToggleLastItem(false);
    }

    experimentalSettingsMenu->addMenuItem(MenuItem("Fast Page Transitions", [this]() {
        this->prefs_.fastTransitions.set(!this->prefs_.fastTransitions.get().value_or(false));
    }));

    if (prefs_.fastTransitions.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(true);
    } else {
        experimentalSettingsMenu->setToggleLastItem(false);
    }

    experimentalSettingsMenu->addMenuItem(MenuItem("Page Transition Arrows", [this]() {
        this->prefs_.pageTransitionArrows.set(
            !this->prefs_.pageTransitionArrows.get().value_or(false));
    }));

    if (prefs_.pageTransitionArrows.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(true);
    } else {
        experimentalSettingsMenu->setToggleLastItem(false);
    }

    experimentalSettingsMenu->addMenuItem(MenuItem("Upload Crashes on Sync", [this]() {
        this->prefs_.noUploadCrashes.set(!this->prefs_.noUploadCrashes.get().value_or(false));
    }));

    // This one is opposite because we want to default it to true and make them
    // turn it off if they don't want it. At least for beta.
    if (prefs_.noUploadCrashes.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(false);
    } else {
        experimentalSettingsMenu->setToggleLastItem(true);
    }

    // Unpair PageTurner
    // Sync Content
    // Turn on Onboarding Mode
    // Turn on Demo Mode
    // Clear Books and Restart
    // Clear Articles and Restart
    // Clear Crash Reports and Restart
    // Onboarding Screens
    // File System Browser
    // Batteries and Temperature
    // Optics Test
    // Turn on EMC Mode
    // Turn off EMC Mode
    // Alignment Setup
    // Set Vcom
    // Set WiFi Power
    // Set Vcom Precise
    // Upload Voltage Log
    // Format FS
    // Power Off

    if (prefs_.showDebugMenu.get().value_or(false)) {
        auto debugSettingsMenu =
            new MenuScreen(power_.battery, *display, styleFamilies.settingsMenu, "Debug");

        debugSettingsMenu->setBackCallback([this, mainMenu]() {
            this->display_->clearDisplay();
            mainMenu->setFirstLoad();
            setCurrentScreen(*mainMenu);
        });

        settingsMenu->addMenuItem(MenuItem("Debug", [this, debugSettingsMenu]() {
            log_d("Loading Debug Settings Menu");
            debugSettingsMenu->setFirstLoad();
            this->setCurrentScreen(*debugSettingsMenu);
        }));

#if CONFIG_USE_TURN_TOUCH_REMOTE
        debugSettingsMenu->addMenuItem(MenuItem("Unpair PageTurner",
                                                [this, debugSettingsMenu, display]() {
            prefs_.pairedTurnTouchAddress.remove();
            prefs_.lastPageTurnerBuild.remove();
            PreferencesStore::lastPageTurnerBattery = 0;
            log_w("Unpaired PageTurner. Rebooting...");
            debugSettingsMenu->showTemporaryMessage(*display, "Unpaired PageTurner! Resetting...",
                                                    2000, false);
            power_.reboot();
        }));
#endif

        debugSettingsMenu->addMenuItem(MenuItem("About", [this, debugSettingsMenu, display]() {
            auto debugAboutScreen = new DebugAboutScreen(*display);
            this->setCurrentScreen(*debugAboutScreen);
            debugAboutScreen->setBackCallback([this, debugAboutScreen, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                display_->clearDisplay();
                setCurrentScreen(*debugSettingsMenu);
                delete debugAboutScreen;
            });
        }));

        if (hasLastQuestion) {
            debugSettingsMenu->addMenuItem(MenuItem("Turn on Demo Mode",
                                                    [this, debugSettingsMenu, display]() {
                log_d("Demo On!");
                prefs_.cesMode.set(true);
                debugSettingsMenu->showTemporaryMessage(*display, "Demo mode on! Resetting...",
                                                        2000, false);
                power_.reboot();
            }));
        }

        debugSettingsMenu->addMenuItem(MenuItem("Reset Preferences",
                                                [this, debugSettingsMenu, display]() {
            prefs_.clear();
            log_w("Preferences cleared. Rebooting...");
            debugSettingsMenu->showTemporaryMessage(*display, "Preferences cleared. Rebooting...",
                                                    2000, false);
            power_.reboot();
        }));

        if (prefs_.allowUsageWhileCharging.get().value_or(false)) {
            debugSettingsMenu->addMenuItem(MenuItem("Disable use while charging",
                                                    [this, debugSettingsMenu, display]() {
                prefs_.allowUsageWhileCharging.remove();
                debugSettingsMenu->showTemporaryMessage(*display, "Disabling use while charging...",
                                                        500, false);
                power_.reboot();
            }));
        } else {
            debugSettingsMenu->addMenuItem(MenuItem("Enable use while charging",
                                                    [this, debugSettingsMenu, display]() {
                prefs_.allowUsageWhileCharging.set(true);
                debugSettingsMenu->showTemporaryMessage(*display, "Enabling use while charging...",
                                                        500, false);
                power_.reboot();
            }));
        }

        debugSettingsMenu->addMenuItem(MenuItem("Clear Books and Restart",
                                                [debugSettingsMenu, display, &localStore]() {
            localStore.deleteReadables();
            debugSettingsMenu->showTemporaryMessage(*display, "Books reset.");
        }));

        debugSettingsMenu->addMenuItem(MenuItem("Clear Articles and Restart",
                                                [debugSettingsMenu, display, &localStore]() {
            localStore.deleteArticles();
            debugSettingsMenu->showTemporaryMessage(*display, "Articles reset.");
        }));

        debugSettingsMenu->addMenuItem(MenuItem("Clear Crash Reports and Restart",
                                                [this, debugSettingsMenu, display, &fileSystem]() {
            FileList coreDumpFiles;
            fileSystem.listDir("/coredumps", coreDumpFiles);
            for (auto &file : coreDumpFiles) {
                log_i("Removing %s", file.c_str());
                fileSystem.remove(StringFormat("%s%s", "/coredumps/", file.c_str()));
            }
            debugSettingsMenu->showTemporaryMessage(*display, "Crash Reports cleared.");
            Delay(1500);
            power_.reboot();
        }));
        debugSettingsMenu->addMenuItem(MenuItem("File System Browser",
                                                [this, display, debugSettingsMenu, &fileSystem]() {
            auto fsMenu = new MenuScreen(power_.battery, *display, debugSettingsMenu->getMyStyle(),
                                         "FS Browser", "Debug Settings");

            handleBrowse(fileSystem, fsMenu);

            setCurrentScreen(*fsMenu);
            fsMenu->setBackCallback([this, fsMenu, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete fsMenu;
            });
        }));

        debugSettingsMenu->addMenuItem(MenuItem("Batteries and Temperature",
                                                [this, display, debugSettingsMenu]() {
            auto batteryScreen = new BatteryScreen(*display, prefs_, power_.battery);
            this->setCurrentScreen(*batteryScreen);
            batteryScreen->setBackCallback([this, batteryScreen, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete batteryScreen;
            });
        }));

        debugSettingsMenu->addMenuItem(MenuItem("Optics Test", [this, debugSettingsMenu]() {
            auto opticsTestScreen = new CalibrationScreen();
            opticsTestScreen->setOpticsTest();
            this->display_->clearDisplay();
            this->setCurrentScreen(*opticsTestScreen);
            opticsTestScreen->setBackCallback([this, opticsTestScreen, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete opticsTestScreen;
            });
        }));

        debugSettingsMenu->addMenuItem(MenuItem("Binocular Optics Test",
                                                [this, debugSettingsMenu]() {
            auto opticsTestScreen = new TestAlignmentScreen();
            this->display_->clearDisplay();
            this->setCurrentScreen(*opticsTestScreen);
            opticsTestScreen->setBackCallback([this, opticsTestScreen, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete opticsTestScreen;
            });
        }));

        SHOW_HEAP();

        if (hasLastQuestion) {
            debugSettingsMenu->addMenuItem(MenuItem("Turn on EMC Mode",
                                                    [this, debugSettingsMenu, display]() {
                log_d("EMC On!");
                prefs_.emcMode.set(true);
                debugSettingsMenu->showTemporaryMessage(*display, "EMC mode on! Resetting...", 2000,
                                                        false);
                power_.reboot();
            }));

            debugSettingsMenu->addMenuItem(MenuItem("Turn off EMC Mode",
                                                    [this, debugSettingsMenu, display]() {
                log_d("EMC Off!");
                prefs_.emcMode.remove();
                debugSettingsMenu->showTemporaryMessage(*display, "EMC mode off! Resetting...",
                                                        2000, false);
                power_.reboot();
            }));
        }
/*
    debugSettingsMenu->addMenuItem(
        MenuItem("Alignment Setup", [this, debugSettingsMenu, display, &lightingController]() {
            auto alignmentScreen = new AlignmentScreen(*display, prefs_, lightingController);
            this->setCurrentScreen(*alignmentScreen);
            alignmentScreen->setBackCallback([this, alignmentScreen, debugSettingsMenu, display]() {
                debugSettingsMenu->setFirstLoad();
                display->clearDisplay();
                setCurrentScreen(*debugSettingsMenu);
                delete alignmentScreen;
            });
        }));
*/
#if CONFIG_DISPLAY_EINKET013TT1
        debugSettingsMenu->addMenuItem(MenuItem("Set Vcom", [this, display, debugSettingsMenu]() {
            auto vcomMenu =
                new MenuScreen(power_.battery, *display, debugSettingsMenu->getMyStyle(),
                               "Vcom Settings", "Debug Settings");
            auto *driver = (dynamic_cast<DisplayEinkET013TT1 *>(display))->getDriver();
            vcomMenu->addMenuItem(MenuItem("0V", [this, vcomMenu, driver]() {
                driver->setVcom(0);
                prefs_.vcom.set(0);
                vcomMenu->setNeedsRerender();
            }));
            vcomMenu->addMenuItem(MenuItem("-1V", [this, vcomMenu, driver]() {
                driver->setVcom(-1000);
                prefs_.vcom.set(-1000);
                vcomMenu->setNeedsRerender();
            }));
            vcomMenu->addMenuItem(MenuItem("-2V", [this, vcomMenu, driver]() {
                driver->setVcom(-2000);
                prefs_.vcom.set(-2000);
                vcomMenu->setNeedsRerender();
            }));
            vcomMenu->addMenuItem(MenuItem("-3V", [this, vcomMenu, driver]() {
                driver->setVcom(-3000);
                prefs_.vcom.set(-3000);
                vcomMenu->setNeedsRerender();
            }));
            vcomMenu->addMenuItem(MenuItem("-4V", [this, vcomMenu, driver]() {
                driver->setVcom(-4000);
                prefs_.vcom.set(-4000);
                vcomMenu->setNeedsRerender();
            }));
            vcomMenu->addMenuItem(MenuItem("-5V", [this, vcomMenu, driver]() {
                driver->setVcom(-5000);
                prefs_.vcom.set(-5000);
                vcomMenu->setNeedsRerender();
            }));
            vcomMenu->addMenuItem(MenuItem("-5.6V", [this, vcomMenu, driver]() {
                driver->setVcom(-5600);
                prefs_.vcom.set(-5600);
                vcomMenu->setNeedsRerender();
            }));
            setCurrentScreen(*vcomMenu);
            vcomMenu->setBackCallback([this, vcomMenu, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete vcomMenu;
            });
        }));
#endif

        debugSettingsMenu->addMenuItem(MenuItem("Set WiFi Power",
                                                [this, display, debugSettingsMenu]() {
            auto wifiPowerMenu =
                new MenuScreen(power_.battery, *display, debugSettingsMenu->getMyStyle(),
                               "WiFi Power Settings", "Debug Settings");

            auto powerToString = [](wifi_power_t power) -> std::string {
                switch (power) {
                case WIFI_POWER_19_5dBm:
                    return "Max (19.5dBm)";
                case WIFI_POWER_16dBm:
                    return "High (16dBm)";
                case WIFI_POWER_11dBm:
                    return "Medium (11dBm)";
                case WIFI_POWER_5dBm:
                    return "Low (5dBm)";
                case WIFI_POWER_MINUS_1dBm:
                    return "Min (-1dBm)";
                default:
                    return "Unknown";
                }
            };

            wifiPowerMenu->setExtraText(powerToString(WiFi::getInstance().getWiFiPower()));

            auto confirmMessage = [this, wifiPowerMenu, display](wifi_power_t power) {
                this->prefs_.wifiPower.set(power);
                wifiPowerMenu->showTemporaryMessage(*display, "Power set", 200, false);
                wifiPowerMenu->back();
            };

            wifiPowerMenu->addMenuItem(MenuItem("Max (19.5dBm)", [confirmMessage]() {
                WiFi::getInstance().setWiFiPower(WIFI_POWER_19_5dBm);
                confirmMessage(WIFI_POWER_19_5dBm);
            }));
            wifiPowerMenu->addMenuItem(MenuItem("High (16dBm)", [confirmMessage]() {
                WiFi::getInstance().setWiFiPower(WIFI_POWER_16dBm);
                confirmMessage(WIFI_POWER_16dBm);
            }));
            wifiPowerMenu->addMenuItem(MenuItem("Medium (11dBm)", [confirmMessage]() {
                WiFi::getInstance().setWiFiPower(WIFI_POWER_11dBm);
                confirmMessage(WIFI_POWER_11dBm);
            }));
            wifiPowerMenu->addMenuItem(MenuItem("Low (5dBm)", [confirmMessage]() {
                WiFi::getInstance().setWiFiPower(WIFI_POWER_5dBm);
                confirmMessage(WIFI_POWER_5dBm);
            }));
            wifiPowerMenu->addMenuItem(MenuItem("Min (-1dBm)", [confirmMessage]() {
                WiFi::getInstance().setWiFiPower(WIFI_POWER_MINUS_1dBm);
                confirmMessage(WIFI_POWER_MINUS_1dBm);
            }));

            int menuIndex = 0;
            switch (WiFi::getInstance().getWiFiPower()) {
            case WIFI_POWER_16dBm:
                menuIndex = 1;
                break;
            case WIFI_POWER_11dBm:
                menuIndex = 2;
                break;
            case WIFI_POWER_5dBm:
                menuIndex = 3;
                break;
            case WIFI_POWER_MINUS_1dBm:
                menuIndex = 4;
                break;
            case WIFI_POWER_19_5dBm:
            default:
                menuIndex = 0;
                break;
            }
            wifiPowerMenu->setCurrSelection(menuIndex);

            setCurrentScreen(*wifiPowerMenu);
            wifiPowerMenu->setBackCallback([this, wifiPowerMenu, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete wifiPowerMenu;
            });
        }));

#if USE_BLE
        debugSettingsMenu->addMenuItem(MenuItem("Set BLE Power",
                                                [this, display, debugSettingsMenu]() {
            static std::unordered_map<esp_power_level_t, std::string> blePowerOptions = {
                {ESP_PWR_LVL_P3, "+3dBm"},
                {ESP_PWR_LVL_N0, "0dBm"},
                {ESP_PWR_LVL_N3, "-3dBm"},
                {ESP_PWR_LVL_N9, "-9dBm"},
#if CONFIG_IDF_TARGET_ESP32S3
                {ESP_PWR_LVL_N18, "-18dBm"},
                {ESP_PWR_LVL_N24, "-24dBm"},
#endif
            };

            auto blePowerMenu =
                new MenuScreen(power_.battery, *display, debugSettingsMenu->getMyStyle(),
                               "BLE Power Settings", "Debug Settings");

            auto powerToString = [](esp_power_level_t power) -> std::string {
                auto it = blePowerOptions.find(power);
                return it != blePowerOptions.end() ? it->second : "Unknown";
            };

            esp_power_level_t currentPower = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
            blePowerMenu->setExtraText(powerToString(currentPower));

            auto confirmMessage =
                [this, blePowerMenu, display, powerToString](esp_power_level_t power) {
                esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, power);
                if (err != ESP_OK) {
                    log_e("esp_ble_tx_power_set ESP_BLE_PWR_TYPE_DEFAULT err:%s",
                          esp_err_to_name(err));
                }
                prefs_.blePower.set(power);
                blePowerMenu->showTemporaryMessage(
                    *display, StringFormat("Power set to %s", powerToString(power).c_str()), 200,
                    false);
                blePowerMenu->back();
            };
            for (const auto &powerItem : blePowerOptions) {
                blePowerMenu->addMenuItem(MenuItem(powerToString(powerItem.first),
                                                   [confirmMessage, power = powerItem.first]() {
                    confirmMessage(power);
                }));
            }

            // Find the currently selected power
            int menuIndex = 0;
            for (const auto &powerItem : blePowerOptions) {
                if (powerItem.first == currentPower) {
                    break;
                }
                menuIndex++;
            }
            blePowerMenu->setCurrSelection(menuIndex);

            setCurrentScreen(*blePowerMenu);
            blePowerMenu->setBackCallback([this, blePowerMenu, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete blePowerMenu;
            });
        }));
#endif

#if CONFIG_DISPLAY_EINKET013TT1
        debugSettingsMenu->addMenuItem(MenuItem("Set Vcom Precise",
                                                [this, display, debugSettingsMenu]() {
            auto vcomPreciseScreen = new VcomAdjustScreen(*display, this->prefs_);
            this->display_->clearDisplay();
            setCurrentScreen(*vcomPreciseScreen);
            vcomPreciseScreen->setBackCallback([this, vcomPreciseScreen, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete vcomPreciseScreen;
            });
        }));
#endif

        debugSettingsMenu->addMenuItem(MenuItem("Format FS",
                                                [this, display, &fileSystem, debugSettingsMenu]() {
            log_w("Formatting...");
            debugSettingsMenu->showTemporaryMessage(
                *display, "Formatting, this will take several minutes.", 2000, false);
            fileSystem.format();
            power_.reboot();
        }));

        debugSettingsMenu->addMenuItem(MenuItem("Power Off", [this, display, debugSettingsMenu]() {
            log_w("Powering off.");
            debugSettingsMenu->showTemporaryMessage(*display, "Powering off.", 2000, false);
            power_.powerOff();
        }));
    }

    if (!(prefs_.tutorialsDeleted.get().value_or(0) & TUTORIAL_SCREEN_SETTINGS)) {
        settingsMenu->addMenuItem(MenuItem("\ue000 Settings Tutorial",
                                           [this, createTutorialScreen]() {
            SpiramString message = SpiRamStringFormat(
                "Welcome to the settings menu. Here you can "
                "change the settings of your Sol Reader. Check out the "
                "experimental settings for features we are beta testing!\n\n\nPress "
                "\ue000 (center) to continue.  ");
            auto tutorialScreen = createTutorialScreen(message, TUTORIAL_SCREEN_SETTINGS);
            this->setCurrentScreen(*tutorialScreen);
        }));
    }

    mainMenu->addSubMenu(settingsMenu);

    if (prefs_.cesMode.get().value_or(false)) {
        auto cesScreen = new DemoScreen(*display, prefs_);

        // reset prefs for of The Last Question readable
        log_d("Starting CES mode readable load");
        auto it = std::find_if(readables.begin(), readables.end(),
                               [](const std::shared_ptr<Readable> &x) {
            return (x->title == "The Last Question");
        });
        if (it == readables.end()) {
            log_e("Could not find readable: %s", "The Last Question");
        } else {
            log_d("Found readable: %s", (*it)->title.c_str());
        }
        auto readableScreen = createReadableScreen(*it);
        if (readableScreen != nullptr) {
            cesScreen->setBackCallback([this, cesScreen, display, readableScreen]() {
                display->clearDisplay();
                this->display_->setFont(this->display_->style.getFontFamily().readableFont);
                log_i("Setting Readable Screen after focus in CES mode");
                this->setCurrentScreen(*readableScreen);
                readableScreen->render(*this->display_);
                delete cesScreen;
            });
            this->setCurrentScreen(*cesScreen);
        } else {
            auto *errorPopup = new MsgPopup(*display, itemErrorMsg_);
            this->setCurrentScreen(*errorPopup);
            errorPopup->setBackCallback([this, errorPopup, mainMenu]() {
                mainMenu->setFirstLoad();
                display_->clearDisplay();
                setCurrentScreen(*mainMenu);
                delete errorPopup;
            });
        }
    } else if (prefs_.emcMode.get().value_or(false)) {
        log_d("Starting EMC mode readable reading turn page every 30s and content sync every 60s");
        auto it = std::find_if(readables.begin(), readables.end(),
                               [](const std::shared_ptr<Readable> &x) {
            return (x->title == "The Last Question");
        });
        if (it == readables.end()) {
            log_e("Could not find readable: %s", "The Last Question");
        } else {
            log_d("Found readable: %s", (*it)->title.c_str());
        }
        auto readableScreen = createReadableScreen(*it);
        if (readableScreen != nullptr) {
            display->clearDisplay();
            this->display_->setFont(this->display_->style.getFontFamily().readableFont);
            this->setCurrentScreen(*readableScreen);
        } else {
            auto *errorPopup = new MsgPopup(*display, itemErrorMsg_);
            this->setCurrentScreen(*errorPopup);
            errorPopup->setBackCallback([this, errorPopup, mainMenu]() {
                mainMenu->setFirstLoad();
                display_->clearDisplay();
                setCurrentScreen(*mainMenu);
                delete errorPopup;
            });
        }
    } else if (currentReadable.has_value()) {
        // Check if we left-off on a readable and if so, reload that readable by default
        log_d("Attempting to find readable: %s", currentReadable.value().c_str());
        // If we previously had a readable loaded, show that same readable by default
        auto booksIterator = std::find_if(readables.begin(), readables.end(),
                                          [&currentReadable](const std::shared_ptr<Readable> &x) {
            return currentReadable == x->uuid;
        });
        auto articlesIterator =
            std::find_if(articles.begin(), articles.end(),
                         [&currentReadable](const std::shared_ptr<Readable> &x) {
            return currentReadable == x->uuid;
            });
        if (booksIterator != readables.end() || articlesIterator != articles.end()) {
            auto it = (booksIterator != readables.end()) ? booksIterator : articlesIterator;
            auto readableScreen = createReadableScreen(*it);
            if (readableScreen != nullptr) {
                this->display_->setFont(this->display_->style.getFontFamily().readableFont);
                this->setCurrentScreen(*readableScreen);
                log_i("Resuming readable %s at place %s", (*it)->title.c_str(),
                      (*it)->place->str().c_str());
                if (articlesIterator != articles.end()) {
                    mainMenu->setTabToArticles();
                } else if (isBible(*it)) {
                    mainMenu->setTabToBible();
                }
            } else {
                auto *errorPopup = new MsgPopup(*display, itemErrorMsg_);
                this->setCurrentScreen(*errorPopup);
                errorPopup->setBackCallback([this, errorPopup, mainMenu]() {
                    mainMenu->setFirstLoad();
                    display_->clearDisplay();
                    setCurrentScreen(*mainMenu);
                    delete errorPopup;
                });
            }
        } else {
            // Assume this readable has gone away (or changed from id to uuid)
            prefs_.lastReadableId.remove();
        }
    }

    // A settings changed caused this restart, so go to the settings tab
    // and clear out that pref.
    if (PreferencesStore::settingsOnRestart) {
        PreferencesStore::settingsOnRestart = false;
        mainMenu->setTabToLast();
    }

    // This still has to be this way rather than else if because the
    // currentReadable.has_value() Could return true but if we don't find that readable, no
    // screen will be set. I asssume this would happen if the FS is updated underneath us to
    // remove that readable.
    if (this->currentScreen_ == nullptr) {
        // If we've already connected to an input device, go straight
        // to the main menu. We will clean this up a bit moving forward
        // but for now it makes it clear when it's connected and when it
        // hasn't on boot.

        // By default, if we don't already have a different first screen ready, show the
        // calibration
        // screen

        setCurrentScreen(*mainMenu);
    }

    // If we are in demo mode, overright whatever the current screen would have been
    // with the demo code and set the back callback to mainMenu.

    SHOW_HEAP();

    // Yes, clang-tidy, it is weird that we are calling `new` and not `delete` and we know this
    // should be a shared_ptr or something. But just leave us alone for now, ok?
    // NOTE: I couldn't figure out how to use
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) in a way that disabled these warnings
    // for these variables
#ifdef __clang_analyzer__
    delete articlesMenu;
    delete booksMenu;
#endif
}

void RootUI::handleBrowse(FileSystem &fileSystem, MenuScreen *fsMenu, const std::string &path) {
    FileList localFiles, localDirs;

    fileSystem.listDirWithSubdirectories(path, localFiles, localDirs);

    fsMenu->resetMenuItems();

    for (const std::string &dir : localDirs) {
        log_d("Next Dir is %s", dir.c_str());
        fsMenu->addMenuItem(MenuItem(StringFormat("%s/", dir.c_str()),
                                     [this, fsMenu, &fileSystem, path, dir]() {
            this->display_->clearDisplay();
            handleBrowse(fileSystem, fsMenu, path + dir + "/");
            fsMenu->setExtraText(StringFormat("Browsing %s", (path + dir + "/").c_str()));
            fsMenu->setNeedsRerender();
        }));
    }

    for (const std::string &file : localFiles) {
        log_d("Next File is %s", file.c_str());
        fsMenu->addMenuItem(MenuItem(file, [this, fsMenu, &fileSystem, path, file]() {
            SFile toSerialOutput = fileSystem.open(path + file);
            while (toSerialOutput.available()) {
                printf("%c", toSerialOutput.read());
            }
            this->display_->clearDisplay();
            fsMenu->setExtraText(StringFormat("output %s to serial", (path + file).c_str()));
            fsMenu->setNeedsRerender();
        }));
    }
}
