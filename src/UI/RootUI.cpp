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
#include "Misc/Platform.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/SolClock.hpp"
#include "Misc/StringUtil.hpp"
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
#include "UI/Menu/GenericSettingsMenuScreen.hpp"
#include "UI/Menu/InactivitySettingMenuScreen.hpp"
#include "UI/Menu/NewMainMenuScreen.hpp"
#include "UI/Menu/NewMenuItemArticle.hpp"
#include "UI/Menu/NewMenuItemBook.hpp"
#include "UI/Menu/NewMenuItemCheckable.hpp"
#include "UI/Menu/NewMenuItemSingleLine.hpp"
#include "UI/Menu/ReadingSettingsMenuScreen.hpp"
#include "UI/Menu/WiFiSettingsMenuScreen.hpp"
#include "UI/MsgPopup.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/OnboardingContentSyncScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/ReleaseNotesScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/StatsScreen.hpp"
#include "UI/TestAlignmentScreen.hpp"
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

void RootUI::setCurrentScreen(const std::shared_ptr<Screen> &screen) {
    screen->setNeedsRerender();
    screenStack_.push(screen);
}

auto RootUI::needsRerender() -> bool {
    if (!screenStack_.empty()) {
        return screenStack_.top()->needsRerender();
    } else {
        return Screen::needsRerender();
    }
};

void RootUI::render(DisplaySystem &display) {
    if (!screenStack_.empty()) {
        screenStack_.top()->render(display);
    }
};

void RootUI::processCommand(RemoteCommand command) {
    if (!screenStack_.empty()) {
        screenStack_.top()->processCommand(command);
    }
}

void RootUI::clearNeedsRerender() {
    if (!screenStack_.empty()) {
        screenStack_.top()->clearNeedsRerender();
    } else {
        Screen::clearNeedsRerender();
    }
}

// NOLINTNEXTLINE(readability-function-size): It's big, I know
void RootUI::setup(DisplaySystem *display, LocalStore &localStore, FileSystem &fileSystem,
                   LightingController &lightingController) {

    display_ = display;
    auto lastMagReading = power_.getLastMagReading();

    HEAP_PROFILE_START(rootUILogBootData);

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

    HEAP_PROFILE_END(rootUILogBootData);

    HEAP_PROFILE_START(rootUIContent);
    auto &readables = localStore.books();
    auto &articles = localStore.articles();
    auto noBooks = readables.empty();
    auto noArticles = articles.empty();
    auto noContent = noBooks && noArticles;

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

    // Bible hack (to show off if we wanted to have different "apps" that had different
    // behavior. For now, it's just the same as a regular book.)
    auto isBible = [](const std::shared_ptr<Readable> &r) -> bool {
        return (r->title == "Psalms and Proverbs" || r->title == "New Testament" ||
                r->title == "Old Testament" || r->title == "Torah");
    };

    HEAP_PROFILE_START(NewMainMenuScreen);
    auto newMainMenu = std::make_shared<NewMainMenuScreen>(*display_, power_.battery);
    HEAP_PROFILE_END(NewMainMenuScreen);

    SIZE_OF(*newMainMenu);

    auto createReadableScreen =
        [this, display, &fileSystem, &lightingController, &localStore](
            const std::shared_ptr<Readable> &readable) -> std::shared_ptr<ReadableScreen> {
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

        Font *font;
        switch (prefs_.font.get().value_or(PREFS_SOL_SMALL)) {
        case PREFS_SOL_MEDIUM:
            font = static_cast<Font *>(&fontFace1);
            break;
        case PREFS_SOL_LARGE:
            font = static_cast<Font *>(&fontFace2);
            break;
        case PREFS_SOL_SMALL:
            font = static_cast<Font *>(&fontFace0);
            break;
        }

        auto readableScreen = std::make_shared<ReadableScreen>(
            stream, *display, readable, this->prefs_, this->telemetry_, fileSystem,
            lightingController, localStore, font);

        this->telemetry_.logMetric(TelemetryField::METRIC_READABLE_OPEN, readable->uuid,
                                   readable->telemetryPlacemarkMetrics());

        // When switching to a readable from a menu, we should clear the display
        // to make the whites really white and the blacks really black.
        display->clearDisplay();
        readableScreen->setBackCallback([this, display, stream, readable]() {
            bool archived = prefs_.readableArchivedChanged.get(readable->shortenedUuid.c_str());
            auto title =
                StringFormat("%s%s", archived ? "Archived: " : "", readable->title.c_str());
            auto displayedTitle =
                StringFormat("%s%s", !archived ? "Archived: " : "", readable->title.c_str());
            log_d("Changing readable title to %s", title.c_str());

            // When leaving a readable to go back to the main menu, do a full clear
            // of the display.
            prefs_.lastReadableId.remove();
            display->clearDisplay();
            screenStack_.pop();

            // TODO: This causes problems but we have to figure out how to update the title on
            // archive.
            std::static_pointer_cast<NewMainMenuScreen>(screenStack_.top())->regenerateCurrentTab();
        });

        return readableScreen;
    };

    auto createTutorialScreen = [this, display](SpiramString text,
                                                int bit) -> std::shared_ptr<TutorialScreen> {
        auto tutorialScreen =
            std::make_shared<TutorialScreen>(*display, this->prefs_, std::move(text), bit);

        tutorialScreen->setBackCallback([this, display]() {
            display->clearDisplay();
            screenStack_.pop();
        });

        return tutorialScreen;
    };

    auto currentReadable = prefs_.lastReadableId.get();

    bool hasLastQuestion = false;
    auto it =
        std::find_if(readables.begin(), readables.end(), [](const std::shared_ptr<Readable> &x) {
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
    auto makeReleaseNotesMenuItem = [this, &localStore, display](const Firmware &firmware) {
        log_i("Found pending firmware update %s", firmware.version.c_str());
        return std::make_shared<NewMenuItemSingleLine>(
            StringFormat("Upgrade Software from %s to %s", SemanticVersionString(),
                         firmware.version.c_str()),
            [this, display, &localStore, firmware]() {
            auto releaseNotesScreen = std::make_shared<ReleaseNotesScreen>(
                *display, this->prefs_, power_, localStore, firmware);
            display->clearDisplay();
            setCurrentScreen(releaseNotesScreen);
            releaseNotesScreen->setBackCallback([this, display]() {
                display->clearDisplay();
                screenStack_.pop();
            });
            });
    };

    auto makeContentSyncMenuItem =
        [this, display, &localStore](const char *title) -> std::shared_ptr<NewMenuItem> {
        return std::make_shared<NewMenuItemSingleLine>(title, [this, display, &localStore]() {
            // If we're not yet paired, show the app screen instead
            if (PreferencesStore::paired == PairedStatus::Unpaired) {
                auto appDownloadScreen = std::make_shared<AppDownloadScreen>(*display);
                this->setCurrentScreen(appDownloadScreen);
                appDownloadScreen->setBackCallback([this, display]() {
                    display->clearDisplay();
                    screenStack_.pop();
                });
                return;
            }

            // Fix this eventually to match the other one once MainMenu is refactored and
            // showTemporaryMessage uses the msgPopup.
            // If wifi & ble have been disabled, show a popup encouraging the user to
            // turn on wifi or BLE to perform the content sync.
            if (this->prefs_.wifiDisabled.get().value_or(false) &&
                this->prefs_.bleDisabled.get().value_or(false)) {
                // TODO: There's a bug when this is called from the Sync Content menu item in the
                // Content menu but because that menu is going away entirely and will be
                // replaced, we'll worry about that once we know where it lands.
                screenStack_.top()->showTemporaryMessage(
                    *display, "Enable WiFi or BLE to perform a Content Sync.", 2000, true);
                screenStack_.top()->setFirstLoad();
                return;
            }

            auto contentSyncScreen = std::make_shared<ContentSyncScreen>(
                *display, localStore, this->prefs_, this->telemetry_, power_);
            contentSyncScreen->setBackCallback([this, display]() {
                display->clearDisplay();
                screenStack_.pop();
                // Any screen that could possibly get to a backCallback without a command
                // being sent through the loop must explicitly render at the end of its
                // backCallback to ensure the screen is properly redrawn.
                if (needsRerender()) {
                    render(*display);
                    clearNeedsRerender();
                }
            });
            this->setCurrentScreen(contentSyncScreen);
            contentSyncScreen->begin();
        });
    };

    SHOW_HEAP();

    auto handleReadableScreenCreation =
        [this, display, createReadableScreen](const std::shared_ptr<Readable> &r) {
        auto readableScreen = createReadableScreen(r);
        if (readableScreen != nullptr) {
            this->setCurrentScreen(readableScreen);
        } else {
            auto errorPopup = std::make_shared<MsgPopup>(*display, itemErrorMsg_);
            this->setCurrentScreen(errorPopup);
            errorPopup->setBackCallback([this]() {
                display_->clearDisplay();
                screenStack_.pop();
            });
        }
    };

    SHOW_HEAP();

    log_i("Loading %zu Pocket articles", articles.size());

    auto recentMenuFactory = [this, &localStore, handleReadableScreenCreation,
                              isBible]() -> MenuItemSharedPtrVector {
        auto &recents = localStore.recents();
        auto menuItems = std::make_shared<MenuItemVector>();
        for (const auto &recent : recents) {
            bool archived = prefs_.readableArchivedChanged.get(recent->shortenedUuid.c_str());
            auto title = StringFormat("%s%s", archived ? "Archived: " : "", recent->title.c_str());

            std::shared_ptr<NewMenuItem> menuItem;
            if (isBible(recent)) {
                menuItem = std::make_shared<NewMenuItemSingleLine>(
                    title, [recent, handleReadableScreenCreation]() {
                        handleReadableScreenCreation(recent);
                    });
            } else if (recent->isArticle()) {
                menuItem = std::make_shared<NewMenuItemArticle>(
                    recent, [recent, handleReadableScreenCreation]() {
                        handleReadableScreenCreation(recent);
                    });
            } else {
                menuItem = std::make_shared<NewMenuItemBook>(
                    recent, [recent, handleReadableScreenCreation]() {
                        handleReadableScreenCreation(recent);
                    });
            }
            menuItems->emplace_back(menuItem);
        }
        return menuItems;
    };

    auto booksMenuFactory = [this, &localStore, handleReadableScreenCreation, isBible,
                             makeReleaseNotesMenuItem, makeContentSyncMenuItem,
                             createTutorialScreen, noContent,
                             noBooks]() -> MenuItemSharedPtrVector {
        auto &books = localStore.books();
        auto menuItems = std::make_shared<MenuItemVector>();

        if (this->prefs_.firmwareLoaded.get().has_value()) {
            Firmware firmware;
            auto upgradeAvailable = localStore.getPendingFirmwareUpdate(firmware);
            if (upgradeAvailable) {
                menuItems->emplace_back(makeReleaseNotesMenuItem(firmware));
            }
        }

        for (const auto &book : books) {
            // Bible books go in their own menu / app
            if (!prefs_.noBible.get().value_or(false) && isBible(book)) {
                continue;
            }
            bool archived = prefs_.readableArchivedChanged.get(book->shortenedUuid.c_str());
            auto title = StringFormat("%s%s", archived ? "Archived: " : "", book->title.c_str());
            menuItems->emplace_back(
                std::make_shared<NewMenuItemBook>(book, [book, handleReadableScreenCreation]() {
                    handleReadableScreenCreation(book);
                }));
        }

        log_d("noContent in the books factory: %d", noContent);
        if (noContent) {
            log_d("Got into noContent in the books factory: %d", noContent);
            std::shared_ptr<Readable> lastQuestion = std::make_shared<Readable>(
                "d7570c42-18a9-4795-a44e-9cee09b9fe3e", "The Last Question", LASTQUESTION_EPUB,
                LASTQUESTION_EPUB_SIZE);
            lastQuestion->authors.emplace_back("91ae1429-c6d4-4b8b-88fa-81041d838f92",
                                               "Isaac Asimov", "");
            if (!prefs_.readableArchivedChanged.get(lastQuestion->shortenedUuid.c_str())) {
                menuItems->emplace_back(std::make_shared<NewMenuItemBook>(
                    lastQuestion, [lastQuestion, handleReadableScreenCreation]() {
                        handleReadableScreenCreation(lastQuestion);
                    }));
            }
        }
        if (noBooks) {
            menuItems->emplace_back(makeContentSyncMenuItem("Sync Content"));
        }

        if (!(prefs_.tutorialsDeleted.get().value_or(0) & TUTORIAL_SCREEN_BOOKS)) {
            menuItems->emplace_back(
                std::make_shared<NewMenuItemSingleLine>("\ue000 Books Tutorial",
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
                this->setCurrentScreen(tutorialScreen);
                }));
        }

        return menuItems;
    };

    auto articlesMenuFactory = [this, &localStore, handleReadableScreenCreation,
                                makeReleaseNotesMenuItem, makeContentSyncMenuItem,
                                createTutorialScreen, noArticles]() -> MenuItemSharedPtrVector {
        auto &articles = localStore.articles();
        auto menuItems = std::make_shared<MenuItemVector>();

        if (this->prefs_.firmwareLoaded.get().has_value()) {
            Firmware firmware;
            auto upgradeAvailable = localStore.getPendingFirmwareUpdate(firmware);
            if (upgradeAvailable) {
                menuItems->emplace_back(makeReleaseNotesMenuItem(firmware));
            }
        }

        for (const auto &article : articles) {
            bool archived = prefs_.readableArchivedChanged.get(article->shortenedUuid.c_str());
            auto title = StringFormat("%s%s", archived ? "Archived: " : "", article->title.c_str());
            menuItems->emplace_back(std::make_shared<NewMenuItemArticle>(
                article, [article, handleReadableScreenCreation]() {
                    handleReadableScreenCreation(article);
                }));
        }

        if (noArticles) {
            menuItems->emplace_back(makeContentSyncMenuItem("Sync Content"));
        }

        if (!(prefs_.tutorialsDeleted.get().value_or(0) & TUTORIAL_SCREEN_ARTICLES)) {
            menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
                "\ue000 Articles Tutorial", [this, createTutorialScreen]() {
                    SpiramString message = SpiRamStringFormat(
                        "Welcome to the Articles menu. This tab "
                        "shows you the articles currently synced to your Sol "
                        "Reader. To add more articles, "
                        "simply connect your Pocket account in the Sol mobile app "
                        "or forward newsletters "
                        "from your inbox to add@solreader.com.\n\n"
                        "Did you know "
                        "you can change the sort order of your articles? "
                        "Simply go to Settings -> Content -> Article Sort Order."
                        "\n\n\nPress \ue000 (center) to "
                        "continue.\n");
                    auto tutorialScreen = createTutorialScreen(message, TUTORIAL_SCREEN_ARTICLES);
                    this->setCurrentScreen(tutorialScreen);
                }));
        }

        return menuItems;
    };

    auto bibleMenuFactory = [this, &localStore, handleReadableScreenCreation, isBible,
                             makeReleaseNotesMenuItem, createTutorialScreen,
                             noContent]() -> MenuItemSharedPtrVector {
        auto &books = localStore.books();
        auto menuItems = std::make_shared<MenuItemVector>();

        if (this->prefs_.firmwareLoaded.get().has_value()) {
            Firmware firmware;
            auto upgradeAvailable = localStore.getPendingFirmwareUpdate(firmware);
            if (upgradeAvailable) {
                menuItems->emplace_back(makeReleaseNotesMenuItem(firmware));
            }
        }

        for (const auto &book : books) {
            // Bible books go in their own menu / app
            if (isBible(book)) {
                bool archived = prefs_.readableArchivedChanged.get(book->shortenedUuid.c_str());
                auto title =
                    StringFormat("%s%s", archived ? "Archived: " : "", book->title.c_str());
                menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
                    std::string(title), [book, handleReadableScreenCreation]() {
                        handleReadableScreenCreation(book);
                    }));
            }
        }

        if (noContent && !prefs_.noBible.get().value_or(false)) {
            std::shared_ptr<Readable> psalmsAndProverbs = std::make_shared<Readable>(
                "6863fd3c-77a9-468e-93fa-f70d8b79029f", "Psalms and Proverbs",
                PSALMSANDPROVERBS_EPUB, PSALMSANDPROVERBS_EPUB_SIZE);
            psalmsAndProverbs->authors.emplace_back("7632d6ec-7339-4752-8672-a966503e1003",
                                                    "Crossway ESV", "");
            if (!prefs_.readableArchivedChanged.get(psalmsAndProverbs->shortenedUuid.c_str())) {
                menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
                    std::string(psalmsAndProverbs->title),
                    [psalmsAndProverbs, handleReadableScreenCreation]() {
                    handleReadableScreenCreation(psalmsAndProverbs);
                    }));
            }
        }

        if (!(prefs_.tutorialsDeleted.get().value_or(0) & TUTORIAL_SCREEN_BIBLE)) {
            menuItems->emplace_back(
                std::make_shared<NewMenuItemSingleLine>("\ue000 Why have a Bible app?",
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
                    " Not your thing? No worries! Just archive these books and the tab "
                    " will disappear.\n\n"
                    " Look for more scripture and meditation Sol Reader tabs down the road!\n\n\n"
                    " Press \ue000 (center) to continue.");
                auto tutorialScreen = createTutorialScreen(message, TUTORIAL_SCREEN_BIBLE);
                this->setCurrentScreen(tutorialScreen);
                }));
        }
        return menuItems;
    };

    auto settingsMenuFactory = [this, display, &localStore, &fileSystem, hasLastQuestion,
                                makeContentSyncMenuItem,
                                &lightingController]() -> MenuItemSharedPtrVector {
        auto menuItems = std::make_shared<MenuItemVector>();

        menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>("Reading",
                                                                        [this, display]() {
            log_d("Loading Reading Settings Menu");

            auto readingSettingsMenu =
                std::make_shared<ReadingSettingsMenuScreen>(*display, "Main Menu", prefs_);

            if (this->prefs_.autoPilot.get().value_or(false)) {
                prefs_.autoPilotSpeed.set(10000);
                readingSettingsMenu->turnOnAutoPilot();
            } else {
                prefs_.autoPilotSpeed.remove();
                readingSettingsMenu->turnOffAutoPilot();
            }

            readingSettingsMenu->setBackCallback([this]() {
                this->display_->clearDisplay();
                screenStack_.pop();
            });

            readingSettingsMenu->addMenuItem(
                std::make_shared<NewMenuItemSingleLine>("Text Size",
                                                        [this, display]() {
                auto fontMenu = std::make_shared<ReadingSettingsMenuScreen>(
                    *display, "Reading Settings", prefs_);
                fontMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Small", [this]() {
                    this->display_->clearDisplay();
                    prefs_.font.set(PREFS_SOL_SMALL);
                    this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                   {{"text_size", "small"}});
                    screenStack_.top()->setNeedsRerender();
                }));
                fontMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Medium", [this]() {
                    this->display_->clearDisplay();
                    prefs_.font.set(PREFS_SOL_MEDIUM);
                    this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                   {{"text_size", "medium"}});
                    screenStack_.top()->setNeedsRerender();
                }));
                fontMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Large", [this]() {
                    this->display_->clearDisplay();
                    prefs_.font.set(PREFS_SOL_LARGE);
                    this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                   {{"text_size", "large"}});
                    screenStack_.top()->setNeedsRerender();
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
                fontMenu->setBackCallback([this]() { screenStack_.pop(); });
                }));

            readingSettingsMenu->addMenuItem(
                std::make_shared<NewMenuItemSingleLine>("Move Page in Display", [this, display]() {
                    log_d("Loading adjust offset");
                    auto offsetAdjust =
                        std::make_shared<OffsetAdjustScreen>(*display, prefs_, this->telemetry_);
                    this->setCurrentScreen(offsetAdjust);
                    log_d("setting up callback");
                    offsetAdjust->setBackCallback([this]() { screenStack_.pop(); });
                    log_d("set up callback");
                }));

            readingSettingsMenu->addMenuItem(
                std::make_shared<NewMenuItemSingleLine>("Resize Page", [this, display]() {
                    log_d("Loading resize box");
                    auto resizeBox =
                        std::make_shared<ResizeBoxScreen>(*display, prefs_, this->telemetry_);
                    this->setCurrentScreen(resizeBox);
                    log_d("setting up callback");
                    resizeBox->setBackCallback([this]() { screenStack_.pop(); });
                    log_d("set up callback");
                }));

            readingSettingsMenu->addMenuItem(
                std::make_shared<NewMenuItemSingleLine>("Line Spacing", [this, display]() {
                    auto lineSpacingScreen =
                        std::make_shared<LineSpacingScreen>(*display, prefs_, this->telemetry_);
                    this->setCurrentScreen(lineSpacingScreen);
                    lineSpacingScreen->setBackCallback([this]() { screenStack_.pop(); });
                }));

            readingSettingsMenu->addMenuItem(std::make_shared<NewMenuItemCheckable>(
                "Dark Mode",
                [this]() {
                this->prefs_.darkMode.set(!this->prefs_.darkMode.get().value_or(false));
                // Reserved for future use? Potentially lighting up and down?
                this->display_->flipInversion();
                // When we invert, we should do a full clear of the display to make sure
                // the blacks are really gone.
                this->display_->clearDisplay();
                prefs_.invertSetting.set(this->display_->getInversion());
                screenStack_.top()->setNeedsRerender();
                screenStack_.top()->setFirstLoad();
                },
                [this]() -> bool { return this->prefs_.darkMode.get().value_or(false); }));

            readingSettingsMenu->addMenuItem(std::make_shared<NewMenuItemCheckable>(
                "Autopilot",
                [this]() {
                this->prefs_.autoPilot.set(!this->prefs_.autoPilot.get().value_or(false));
                if (this->prefs_.autoPilot.get().value_or(false)) {
                    prefs_.autoPilotSpeed.set(10000);
                    // cast this to a readable
                    std::static_pointer_cast<ReadingSettingsMenuScreen>(screenStack_.top())
                        ->turnOnAutoPilot();
                } else {
                    prefs_.autoPilotSpeed.remove();
                    std::static_pointer_cast<ReadingSettingsMenuScreen>(screenStack_.top())
                        ->turnOffAutoPilot();
                }
                screenStack_.top()->setNeedsRerender();
                screenStack_.top()->setFirstLoad();
                },
                [this]() -> bool { return this->prefs_.autoPilot.get().value_or(false); }));

            readingSettingsMenu->setFirstLoad();
            this->setCurrentScreen(readingSettingsMenu);
        }));

        menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
            "Content", [this, display, &localStore, makeContentSyncMenuItem]() {
                log_d("Loading Reading Settings Menu");

                auto contentSettingsMenu =
                    std::make_shared<GenericSettingsMenuScreen>(*display_, "Main Menu");

                for (auto modelType : {ReadablesModelType::Book, ReadablesModelType::Article}) {
                    auto sortOrderMenuItem = std::make_shared<NewMenuItemSingleLine>(
                        modelType == ReadablesModelType::Book ? "Book Sort Order"
                                                              : "Article Sort Order",
                        [this, &localStore, display, modelType]() {
                    auto sortMenu =
                        std::make_shared<GenericSettingsMenuScreen>(*display, "Content Settings");
                    sortMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Alphabetical", [this, &localStore, display, modelType]() {
                            if (modelType == ReadablesModelType::Book) {
                                this->prefs_.bookSortCriteria.set(
                                    static_cast<int>(SortCriteria::Alphabetical));
                                this->prefs_.bookSortDirection.set(
                                    static_cast<int>(SortDirection::Ascending));
                                localStore.saveReadables(localStore.books());
                            } else if (modelType == ReadablesModelType::Article) {
                                this->prefs_.articleSortCriteria.set(
                                    static_cast<int>(SortCriteria::Alphabetical));
                                this->prefs_.articleSortDirection.set(
                                    static_cast<int>(SortDirection::Ascending));
                                localStore.saveReadables(localStore.articles());
                            }
                            screenStack_.top()->showTemporaryMessage(*display, settingsRestartMsg_,
                                                                     500, false);
                            this->power_.reboot();
                        }));
                    sortMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Date added, most recent", [this, &localStore, display, modelType]() {
                            if (modelType == ReadablesModelType::Book) {
                                this->prefs_.bookSortCriteria.set(
                                    static_cast<int>(SortCriteria::DateModified));
                                this->prefs_.bookSortDirection.set(
                                    static_cast<int>(SortDirection::Descending));
                                localStore.saveReadables(localStore.books());
                            } else if (modelType == ReadablesModelType::Article) {
                                this->prefs_.articleSortCriteria.set(
                                    static_cast<int>(SortCriteria::DateModified));
                                this->prefs_.articleSortDirection.set(
                                    static_cast<int>(SortDirection::Descending));
                                localStore.saveReadables(localStore.articles());
                            }
                            screenStack_.top()->showTemporaryMessage(*display, settingsRestartMsg_,
                                                                     500, false);
                            this->power_.reboot();
                        }));
                    sortMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Date added, least recent", [this, &localStore, display, modelType]() {
                            if (modelType == ReadablesModelType::Book) {
                                this->prefs_.bookSortCriteria.set(
                                    static_cast<int>(SortCriteria::DateModified));
                                this->prefs_.bookSortDirection.set(
                                    static_cast<int>(SortDirection::Ascending));
                                localStore.saveReadables(localStore.books());
                            } else if (modelType == ReadablesModelType::Article) {
                                this->prefs_.articleSortCriteria.set(
                                    static_cast<int>(SortCriteria::DateModified));
                                this->prefs_.articleSortDirection.set(
                                    static_cast<int>(SortDirection::Ascending));
                                localStore.saveReadables(localStore.articles());
                            }
                            screenStack_.top()->showTemporaryMessage(*display, settingsRestartMsg_,
                                                                     500, false);
                            this->power_.reboot();
                        }));
                    sortMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Last read, most recent", [this, &localStore, display, modelType]() {
                            if (modelType == ReadablesModelType::Book) {
                                this->prefs_.bookSortCriteria.set(
                                    static_cast<int>(SortCriteria::LastRead));
                                this->prefs_.bookSortDirection.set(
                                    static_cast<int>(SortDirection::Descending));
                                localStore.saveReadables(localStore.books());
                            } else if (modelType == ReadablesModelType::Article) {
                                this->prefs_.articleSortCriteria.set(
                                    static_cast<int>(SortCriteria::LastRead));
                                this->prefs_.articleSortDirection.set(
                                    static_cast<int>(SortDirection::Descending));
                                localStore.saveReadables(localStore.articles());
                            }
                            screenStack_.top()->showTemporaryMessage(*display, settingsRestartMsg_,
                                                                     500, false);
                            this->power_.reboot();
                        }));
                    sortMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Last read, least recent", [this, &localStore, display, modelType]() {
                            if (modelType == ReadablesModelType::Book) {
                                this->prefs_.bookSortCriteria.set(
                                    static_cast<int>(SortCriteria::LastRead));
                                this->prefs_.bookSortDirection.set(
                                    static_cast<int>(SortDirection::Ascending));
                                localStore.saveReadables(localStore.books());
                            } else if (modelType == ReadablesModelType::Article) {
                                this->prefs_.articleSortCriteria.set(
                                    static_cast<int>(SortCriteria::LastRead));
                                this->prefs_.articleSortDirection.set(
                                    static_cast<int>(SortDirection::Ascending));
                                localStore.saveReadables(localStore.articles());
                            }
                            screenStack_.top()->showTemporaryMessage(*display, settingsRestartMsg_,
                                                                     500, false);
                            this->power_.reboot();
                        }));
                    sortMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Random", [this, &localStore, display, modelType]() {
                            if (modelType == ReadablesModelType::Book) {
                                this->prefs_.bookSortCriteria.set(
                                    static_cast<int>(SortCriteria::Random));
                                this->prefs_.bookSortDirection.set(
                                    static_cast<int>(SortDirection::Ascending));
                                localStore.saveReadables(localStore.books());
                            } else if (modelType == ReadablesModelType::Article) {
                                this->prefs_.articleSortCriteria.set(
                                    static_cast<int>(SortCriteria::Random));
                                this->prefs_.articleSortDirection.set(
                                    static_cast<int>(SortDirection::Ascending));
                                localStore.saveReadables(localStore.articles());
                            }
                            screenStack_.top()->showTemporaryMessage(*display, settingsRestartMsg_,
                                                                     500, false);
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
                    sortMenu->setSelectedIndex(menuIndex);
                    setCurrentScreen(sortMenu);
                    sortMenu->setBackCallback([this]() { screenStack_.pop(); });
                        });
                    contentSettingsMenu->addMenuItem(sortOrderMenuItem);
                }

                contentSettingsMenu->addMenuItem(makeContentSyncMenuItem("Sync Content Now"));

                contentSettingsMenu->setBackCallback([this]() {
                    this->display_->clearDisplay();
                    screenStack_.pop();
                });

                contentSettingsMenu->setFirstLoad();
                this->setCurrentScreen(contentSettingsMenu);
            }));

        menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
            "System", [this, display, &lightingController, &localStore, &fileSystem]() {
                log_d("Loading System Settings Menu");

                auto systemSettingsMenu =
                    std::make_shared<GenericSettingsMenuScreen>(*display, "Main Menu");

                systemSettingsMenu->setBackCallback([this]() {
                    this->display_->clearDisplay();
                    screenStack_.pop();
                });

                systemSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("About", [this, display]() {
                        auto aboutScreen = std::make_shared<AboutScreen>(*display, prefs_, power_);
                        this->setCurrentScreen(aboutScreen);
                        aboutScreen->setBackCallback([this]() {
                            display_->clearDisplay();
                            screenStack_.pop();
                        });
                    }));

                // Brightness and contrast are going to move elsewhere
                systemSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "Brightness", [this, display, &lightingController]() {
                        auto lightingAdjustScreen = std::make_shared<LightingAdjustScreen>(
                            *display, this->prefs_, lightingController, this->telemetry_);
                        this->display_->clearDisplay();
                        setCurrentScreen(lightingAdjustScreen);
                        lightingAdjustScreen->setBackCallback([this]() { screenStack_.pop(); });
                    }));

#if CONFIG_DISPLAY_EINKET013TT1
                systemSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "Contrast", [this, display, &lightingController]() {
                        auto contrastAdjustScreen = std::make_shared<ContrastAdjustScreen>(
                            *display, this->prefs_, lightingController, this->telemetry_);
                        this->display_->clearDisplay();
                        setCurrentScreen(contrastAdjustScreen);
                        contrastAdjustScreen->setBackCallback([this]() { screenStack_.pop(); });
                    }));
#endif

                systemSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "Legal & Regulatory", [this, display]() {
                        auto legalScreen = std::make_shared<LegalScreen>(*display, prefs_, power_);
                        this->setCurrentScreen(legalScreen);
                        legalScreen->setBackCallback([this]() {
                            display_->clearDisplay();
                            screenStack_.pop();
                        });
                    }));

                systemSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Firmware Update",
                                                            [this, display]() {
                // If we don't have WiFi configured, show the QR code instead
                if (!WiFi::getInstance().hasWiFiCredentials()) {
                    auto appDownloadScreen = std::make_shared<AppDownloadScreen>(*display);
                    this->setCurrentScreen(appDownloadScreen);
                    appDownloadScreen->setBackCallback([this, display]() {
                        display->clearDisplay();
                        screenStack_.pop();
                    });
                    return;
                }

                std::string turnOnWifiMsg =
                    "Enable WiFi from Settings -> Wifi and Bluetooth to perform a Firmware Update";

                // If wifi has been disabled, show a popup encouraging the user to
                // turn on wifi to perform the firmware sync.
                if (this->prefs_.wifiDisabled.get().value_or(false)) {
                    screenStack_.top()->showTemporaryMessage(*display, turnOnWifiMsg, 2000, true);
                    screenStack_.top()->setFirstLoad();
                    return;
                }

                auto firmwareUpdateScreen =
                    std::make_shared<FirmwareUpdateScreen>(*display, telemetry_, prefs_);
                this->setCurrentScreen(firmwareUpdateScreen);
                firmwareUpdateScreen->setBackCallback([this]() { screenStack_.pop(); });
                firmwareUpdateScreen->begin();
                    }));

                systemSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Manifesto", [this, display]() {
                        auto demoScreen = std::make_shared<DemoScreen>(*display, prefs_);
                        this->setCurrentScreen(demoScreen);
                        demoScreen->setBackCallback([this, display]() {
                            display->clearDisplay();
                            screenStack_.pop();
                        });
                    }));

                systemSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Inactivity Timer",
                                                            [this, display]() {
                auto inactivityMenu =
                    std::make_shared<InactivitySettingMenuScreen>(*display, "System Settings");

                auto updateExtraText = [this]() {
                    std::string message = std::string(SpiRamStringFormat(
                        "Sleep after %d minutes of inactivity.",
                        prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES)));
                    std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                        ->setExtraText(message);
                };

                // This used to be in a lambda but that leaks memory passing around the
                // inactivityMenuItem as a shared pointer.
                inactivityMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("5 minutes", [this, updateExtraText]() {
                        this->display_->clearDisplay();
                        prefs_.inactivityMinutes.set(5);
                        updateExtraText();
                        std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->requireRestart();
                        this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                       {{"inactivity_minutes", 5}});
                        std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));

                inactivityMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "10 minutes", [this, updateExtraText]() {
                        this->display_->clearDisplay();
                        prefs_.inactivityMinutes.set(10);
                        updateExtraText();
                        std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->requireRestart();
                        this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                       {{"inactivity_minutes", 10}});
                        std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));

                inactivityMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "15 minutes", [this, updateExtraText]() {
                        this->display_->clearDisplay();
                        prefs_.inactivityMinutes.set(15);
                        updateExtraText();
                        std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->requireRestart();
                        this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                       {{"inactivity_minutes", 15}});
                        std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));

                inactivityMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "20 minutes", [this, updateExtraText]() {
                        this->display_->clearDisplay();
                        prefs_.inactivityMinutes.set(20);
                        updateExtraText();
                        std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->requireRestart();
                        this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                       {{"inactivity_minutes", 20}});
                        std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));

                inactivityMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "30 minutes", [this, updateExtraText]() {
                        this->display_->clearDisplay();
                        prefs_.inactivityMinutes.set(30);
                        updateExtraText();
                        std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->requireRestart();
                        this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                       {{"inactivity_minutes", 30}});
                        std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));
                int menuIndex = 0;
                if (prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES) == 5) {
                    menuIndex = 0;
                } else if (prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES) ==
                           10) {
                    menuIndex = 1;
                } else if (prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES) ==
                           15) {
                    menuIndex = 2;
                } else if (prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES) ==
                           20) {
                    menuIndex = 3;
                } else if (prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES) ==
                           30) {
                    menuIndex = 4;
                }
                updateExtraText();
                inactivityMenu->setSelectedIndex(menuIndex);
                setCurrentScreen(inactivityMenu);

                inactivityMenu->setBackCallback([this, display]() {
                    if (std::static_pointer_cast<InactivitySettingMenuScreen>(screenStack_.top())
                            ->getRequireRestart()) {
                        PreferencesStore::settingsOnRestart = true;
                        screenStack_.top()->showTemporaryMessage(*display, settingsRestartMsg_,
                                                                 1500, false);
                        this->power_.reboot();
                    } else {
                        screenStack_.pop();
                    }
                });
                    }));

                systemSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "Clear Content and Restart", [this, display, &localStore]() {
                        localStore.deleteReadables(false);
                        localStore.deleteArticles(false);
                        screenStack_.top()->showTemporaryMessage(
                            *display, "Content cleared. Restarting...", 2000, false);
                        power_.reboot();
                    }));

                systemSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("WiFi and Bluetooth",
                                                            [this, display]() {
                auto wifiSettingsMenuScreen = std::make_shared<WiFiSettingsMenuScreen>(*display);

                wifiSettingsMenuScreen->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("WiFi Scan", [this, display]() {
                        auto wiFiScanScreen =
                            std::make_shared<WiFiScanScreen>(*display, WiFi::getInstance());
                        wiFiScanScreen->begin();
                        this->setCurrentScreen(wiFiScanScreen);
                        wiFiScanScreen->setBackCallback([this, display]() {
                            display->clearDisplay();
                            screenStack_.pop();
                        });
                    }));

                wifiSettingsMenuScreen->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "Clear WiFi Settings", [this, display]() {
                        WiFi::getInstance().clearWiFiCredentials();
                        log_w("WiFi settings cleared.");
                        std::static_pointer_cast<WiFiSettingsMenuScreen>(screenStack_.top())
                            ->loadWifiSettings();
                        screenStack_.top()->showTemporaryMessage(*display, "WiFi settings cleared.",
                                                                 2000, true);
                        screenStack_.top()->setFirstLoad();
                    }));

                wifiSettingsMenuScreen->addMenuItem(std::make_shared<NewMenuItemCheckable>(
                    "WiFi is Enabled",
                    [this]() {
                    if (this->prefs_.wifiDisabled.get().value_or(false)) {
                        this->prefs_.wifiDisabled.set(false);
                    } else {
                        this->prefs_.wifiDisabled.set(true);
                    }
                    std::static_pointer_cast<WiFiSettingsMenuScreen>(screenStack_.top())
                        ->setNeedsRerender();
                    },
                    [this]() -> bool { return !this->prefs_.wifiDisabled.get().value_or(false); }));

                wifiSettingsMenuScreen->addMenuItem(std::make_shared<NewMenuItemCheckable>(
                    "Bluetooth Sync is Enabled",
                    [this]() {
                    if (this->prefs_.bleDisabled.get().value_or(false)) {
                        this->prefs_.bleDisabled.set(false);
                    } else {
                        this->prefs_.bleDisabled.set(true);
                    }
                    std::static_pointer_cast<WiFiSettingsMenuScreen>(screenStack_.top())
                        ->setNeedsRerender();
                    },
                    [this]() -> bool { return !this->prefs_.bleDisabled.get().value_or(false); }));

                wifiSettingsMenuScreen->setBackCallback([this, display]() {
                    display->clearDisplay();
                    screenStack_.pop();
                });

                wifiSettingsMenuScreen->setFirstLoad();
                setCurrentScreen(wifiSettingsMenuScreen);
                    }));

                systemSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "Factory Defaults", [this, display, &fileSystem]() {
                        SoftFactoryReset(fileSystem);
                        log_w("NVS and FS cleared. Restarting...");
                        screenStack_.top()->showTemporaryMessage(*display,
                                                                 "Resetting Device! Restarting...");
                        power_.reboot();
                    }));

                systemSettingsMenu->setFirstLoad();
                this->setCurrentScreen(systemSettingsMenu);
            }));

        menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>("Experimental",
                                                                        [this, display]() {
            log_d("Loading Experimental Settings Menu");
            auto experimentalSettingsMenu =
                std::make_shared<GenericSettingsMenuScreen>(*display, "Main Menu");

            experimentalSettingsMenu->setBackCallback([this]() {
                this->display_->clearDisplay();
                screenStack_.pop();
            });

            experimentalSettingsMenu->addMenuItem(std::make_shared<NewMenuItemCheckable>(
                "Show Summary",
                [this]() {
                this->prefs_.noShowPartialChapterSummary.set(
                    !this->prefs_.noShowPartialChapterSummary.get().value_or(false));
                screenStack_.top()->setNeedsRerender();
                },
                [this]() -> bool {
                    return !this->prefs_.noShowPartialChapterSummary.get().value_or(false);
                }));

            experimentalSettingsMenu->addMenuItem(std::make_shared<NewMenuItemCheckable>(
                "Fast Page Transitions",
                [this]() {
                this->prefs_.fastTransitions.set(
                    !this->prefs_.fastTransitions.get().value_or(false));
                screenStack_.top()->setNeedsRerender();
                },
                [this]() -> bool { return this->prefs_.fastTransitions.get().value_or(false); }));

            experimentalSettingsMenu->setFirstLoad();
            this->setCurrentScreen(experimentalSettingsMenu);
        }));

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

            menuItems->emplace_back(std::make_shared<NewMenuItemSingleLine>(
                "Debug",
                [this, display, hasLastQuestion, &localStore, &fileSystem]() {
                log_d("Loading System Settings Menu");

                auto debugSettingsMenu =
                    std::make_shared<GenericSettingsMenuScreen>(*display, "Debug");

                debugSettingsMenu->setBackCallback([this]() {
                    this->display_->clearDisplay();
                    screenStack_.pop();
                });

#if CONFIG_USE_TURN_TOUCH_REMOTE
                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Unpair PageTurner", [this, display]() {
                        prefs_.pairedTurnTouchAddress.remove();
                        prefs_.lastPageTurnerBuild.remove();
                        PreferencesStore::lastPageTurnerBattery = 0;
                        log_w("Unpaired PageTurner. Restarting...");
                        screenStack_.top()->showTemporaryMessage(
                            *display, "Unpaired PageTurner! Resetting...", 2000, false);
                        power_.reboot();
                    }));
#endif

                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("About", [this, display]() {
                        auto debugAboutScreen = std::make_shared<DebugAboutScreen>(*display);
                        debugAboutScreen->setBackCallback([this]() {
                            display_->clearDisplay();
                            screenStack_.pop();
                        });
                        this->setCurrentScreen(debugAboutScreen);
                    }));

                if (hasLastQuestion) {
                    debugSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Turn on Demo Mode", [this, display]() {
                            log_d("Demo On!");
                            prefs_.cesMode.set(true);
                            screenStack_.top()->showTemporaryMessage(
                                *display, "Demo mode on! Resetting...", 2000, false);
                            power_.reboot();
                        }));
                }

                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Reset Preferences", [this, display]() {
                        prefs_.clear();
                        log_w("Preferences cleared. Restarting...");
                        screenStack_.top()->showTemporaryMessage(
                            *display, "Preferences cleared. Restarting...", 2000, false);
                        power_.reboot();
                    }));

                if (prefs_.allowUsageWhileCharging.get().value_or(false)) {
                    debugSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Disable use while charging", [this, display]() {
                            prefs_.allowUsageWhileCharging.remove();
                            screenStack_.top()->showTemporaryMessage(
                                *display, "Disabling use while charging...", 500, false);
                            power_.reboot();
                        }));
                } else {
                    debugSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Enable use while charging", [this, display]() {
                            prefs_.allowUsageWhileCharging.set(true);
                            screenStack_.top()->showTemporaryMessage(
                                *display, "Enabling use while charging...", 500, false);
                            power_.reboot();
                        }));
                }

                debugSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "Clear Books and Restart", [this, display, &localStore]() {
                        localStore.deleteReadables();
                        screenStack_.top()->showTemporaryMessage(*display, "Books reset.");
                    }));

                debugSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "Clear Articles and Restart", [this, display, &localStore]() {
                        localStore.deleteArticles();
                        screenStack_.top()->showTemporaryMessage(*display, "Articles reset.");
                    }));

                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Clear Crash Reports and Restart",
                                                            [this, display, &fileSystem]() {
                    FileList coreDumpFiles;
                    fileSystem.listDir("/coredumps", coreDumpFiles);
                    for (auto &file : coreDumpFiles) {
                        log_i("Removing %s", file.c_str());
                        fileSystem.remove(StringFormat("%s%s", "/coredumps/", file.c_str()));
                    }
                    screenStack_.top()->showTemporaryMessage(*display, "Crash Reports cleared.");
                    Delay(1500);
                    power_.reboot();
                    }));
                debugSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "File System Browser", [this, display, &fileSystem]() {
                        auto fsMenu =
                            std::make_shared<GenericSettingsMenuScreen>(*display, "Debug Settings");

                        handleBrowse(fileSystem, fsMenu);

                        setCurrentScreen(fsMenu);
                        fsMenu->setBackCallback([this]() { screenStack_.pop(); });
                    }));

                debugSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "Batteries and Temperature", [this, display]() {
                        auto batteryScreen =
                            std::make_shared<BatteryScreen>(*display, prefs_, power_.battery);
                        this->setCurrentScreen(batteryScreen);
                        batteryScreen->setBackCallback([this]() { screenStack_.pop(); });
                    }));

                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Optics Test", [this]() {
                        auto opticsTestScreen = std::make_shared<CalibrationScreen>();
                        opticsTestScreen->setOpticsTest();
                        this->display_->clearDisplay();
                        this->setCurrentScreen(opticsTestScreen);
                        opticsTestScreen->setBackCallback([this]() { screenStack_.pop(); });
                    }));

                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Binocular Optics Test", [this]() {
                        auto opticsTestScreen = std::make_shared<TestAlignmentScreen>();
                        this->display_->clearDisplay();
                        this->setCurrentScreen(opticsTestScreen);
                        opticsTestScreen->setBackCallback([this]() { screenStack_.pop(); });
                    }));

                SHOW_HEAP();

                if (hasLastQuestion) {
                    debugSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Turn on EMC Mode", [this, display]() {
                            log_d("EMC On!");
                            prefs_.emcMode.set(true);
                            screenStack_.top()->showTemporaryMessage(
                                *display, "EMC mode on! Resetting...", 2000, false);
                            power_.reboot();
                        }));

                    debugSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Turn off EMC Mode", [this, display]() {
                            log_d("EMC Off!");
                            prefs_.emcMode.remove();
                            screenStack_.top()->showTemporaryMessage(
                                *display, "EMC mode off! Resetting...", 2000, false);
                            power_.reboot();
                        }));
                }
/*
    debugSettingsMenu->addMenuItem(
        std::make_shared<NewMenuItemSingleLine>("Alignment Setup", [this,
   display, &lightingController]() { auto alignmentScreen =
   std::make_shared<AlignmentScreen>(*display, prefs_, lightingController);
   this->setCurrentScreen(alignmentScreen); alignmentScreen->setBackCallback([this,
   display]() {  display->clearDisplay();
                screenStack_.pop();
            });
        }));
*/
#if CONFIG_DISPLAY_EINKET013TT1
                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Set Vcom",
                                                            [this, display]() {
                    auto vcomMenu =
                        std::make_shared<GenericSettingsMenuScreen>(*display, "Debug Settings");
                    auto *driver = (dynamic_cast<DisplayEinkET013TT1 *>(display))->getDriver();
                    vcomMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("0V",
                                                                                  [this, driver]() {
                        driver->setVcom(0);
                        prefs_.vcom.set(0);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("-1V",
                                                                                  [this, driver]() {
                        driver->setVcom(-1000);
                        prefs_.vcom.set(-1000);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("-2V",
                                                                                  [this, driver]() {
                        driver->setVcom(-2000);
                        prefs_.vcom.set(-2000);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("-3V",
                                                                                  [this, driver]() {
                        driver->setVcom(-3000);
                        prefs_.vcom.set(-3000);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("-4V",
                                                                                  [this, driver]() {
                        driver->setVcom(-4000);
                        prefs_.vcom.set(-4000);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("-5V",
                                                                                  [this, driver]() {
                        driver->setVcom(-5000);
                        prefs_.vcom.set(-5000);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("-5.6V",
                                                                                  [this, driver]() {
                        driver->setVcom(-5600);
                        prefs_.vcom.set(-5600);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(screenStack_.top())
                            ->setNeedsRerender();
                    }));
                    setCurrentScreen(vcomMenu);
                    vcomMenu->setBackCallback([this]() { screenStack_.pop(); });
                    }));
#endif

                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Set WiFi Power",
                                                            [this, display]() {
                    auto wifiPowerMenu =
                        std::make_shared<GenericSettingsMenuScreen>(*display, "Debug Settings");

                    // auto powerToString = [](wifi_power_t power) -> std::string {
                    //     switch (power) {
                    //     case WIFI_POWER_19_5dBm:
                    //         return "Max (19.5dBm)";
                    //     case WIFI_POWER_16dBm:
                    //         return "High (16dBm)";
                    //     case WIFI_POWER_11dBm:
                    //         return "Medium (11dBm)";
                    //     case WIFI_POWER_5dBm:
                    //         return "Low (5dBm)";
                    //     case WIFI_POWER_MINUS_1dBm:
                    //         return "Min (-1dBm)";
                    //     default:
                    //         return "Unknown";
                    //     }
                    // };

                    // TODO Add this to GenericSettingsMenuScreen perhaps?
                    // wifiPowerMenu->setExtraText(powerToString(WiFi::getInstance().getWiFiPower()));

                    auto confirmMessage = [this, display](wifi_power_t power) {
                        this->prefs_.wifiPower.set(power);
                        screenStack_.top()->showTemporaryMessage(*display, "Power set", 200, false);
                        screenStack_.top()->back();
                    };

                    wifiPowerMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Max (19.5dBm)", [confirmMessage]() {
                            WiFi::getInstance().setWiFiPower(WIFI_POWER_19_5dBm);
                            confirmMessage(WIFI_POWER_19_5dBm);
                        }));
                    wifiPowerMenu->addMenuItem(
                        std::make_shared<NewMenuItemSingleLine>("High (16dBm)", [confirmMessage]() {
                            WiFi::getInstance().setWiFiPower(WIFI_POWER_16dBm);
                            confirmMessage(WIFI_POWER_16dBm);
                        }));
                    wifiPowerMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                        "Medium (11dBm)", [confirmMessage]() {
                            WiFi::getInstance().setWiFiPower(WIFI_POWER_11dBm);
                            confirmMessage(WIFI_POWER_11dBm);
                        }));
                    wifiPowerMenu->addMenuItem(
                        std::make_shared<NewMenuItemSingleLine>("Low (5dBm)", [confirmMessage]() {
                            WiFi::getInstance().setWiFiPower(WIFI_POWER_5dBm);
                            confirmMessage(WIFI_POWER_5dBm);
                        }));
                    wifiPowerMenu->addMenuItem(
                        std::make_shared<NewMenuItemSingleLine>("Min (-1dBm)", [confirmMessage]() {
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
                    wifiPowerMenu->setSelectedIndex(menuIndex);

                    setCurrentScreen(wifiPowerMenu);
                    wifiPowerMenu->setBackCallback([this]() { screenStack_.pop(); });
                    }));

#if USE_BLE
                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Set BLE Power",
                                                            [this, display]() {
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
                        std::make_shared<GenericSettingsMenuScreen>(*display, "Debug Settings");

                    auto powerToString = [](esp_power_level_t power) -> std::string {
                        auto it = blePowerOptions.find(power);
                        return it != blePowerOptions.end() ? it->second : "Unknown";
                    };

                    esp_power_level_t currentPower = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);

                    // TODO: Maybe add this
                    // blePowerMenu->setExtraText(powerToString(currentPower));

                    auto confirmMessage = [this, display, powerToString](esp_power_level_t power) {
                        esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, power);
                        if (err != ESP_OK) {
                            log_e("esp_ble_tx_power_set ESP_BLE_PWR_TYPE_DEFAULT err:%s",
                                  esp_err_to_name(err));
                        }
                        prefs_.blePower.set(power);
                        screenStack_.top()->showTemporaryMessage(
                            *display, StringFormat("Power set to %s", powerToString(power).c_str()),
                            200, false);
                        screenStack_.top()->back();
                    };
                    for (const auto &powerItem : blePowerOptions) {
                        blePowerMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                            powerToString(powerItem.first),
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
                    blePowerMenu->setSelectedIndex(menuIndex);

                    setCurrentScreen(blePowerMenu);
                    blePowerMenu->setBackCallback([this]() { screenStack_.pop(); });
                    }));
#endif

#if CONFIG_DISPLAY_EINKET013TT1
                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Set Vcom Precise", [this, display]() {
                        auto vcomPreciseScreen =
                            std::make_shared<VcomAdjustScreen>(*display, this->prefs_);
                        this->display_->clearDisplay();
                        setCurrentScreen(vcomPreciseScreen);
                        vcomPreciseScreen->setBackCallback([this]() { screenStack_.pop(); });
                    }));
#endif

                debugSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
                    "Format FS", [this, display, &fileSystem]() {
                        log_w("Formatting...");
                        screenStack_.top()->showTemporaryMessage(
                            *display, "Formatting, this will take several minutes.", 2000, false);
                        fileSystem.format();
                        power_.reboot();
                    }));

                debugSettingsMenu->addMenuItem(
                    std::make_shared<NewMenuItemSingleLine>("Power Off", [this, display]() {
                        log_w("Powering off.");
                        screenStack_.top()->showTemporaryMessage(*display, "Powering off.", 2000,
                                                                 false);
                        power_.powerOff();
                    }));

                debugSettingsMenu->setFirstLoad();
                this->setCurrentScreen(debugSettingsMenu);
                }));
        }

        return menuItems;
    };

    if (!localStore.recents().empty()) {
        newMainMenu->addTab("Recent", recentMenuFactory);
    }
    if (!booksMenuFactory()->empty() && !prefs_.noBooks.get().value_or(false)) {
        newMainMenu->addTab("Books", booksMenuFactory);
    }
    if (!articlesMenuFactory()->empty() && !prefs_.noArticles.get().value_or(false)) {
        newMainMenu->addTab("Articles", articlesMenuFactory);
    }

    if (!bibleMenuFactory()->empty() && !prefs_.noBible.get().value_or(false)) {
        newMainMenu->addTab("Bible", bibleMenuFactory);
    }

    newMainMenu->addTab("Settings", settingsMenuFactory);

    newMainMenu->populateMenuView();

    SHOW_HEAP();

    HEAP_PROFILE_END(rootUIContent);

    HEAP_PROFILE_START(rootUISettings);

#if 0
    if (prefs_.fontsLibrary.get().value_or(false)) {
        readingSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Font Type (Experimental)",
                                                  [this, display, readingSettingsMenu]() {
            auto fontTypeMenu =
                std::make_shared<GenericSettingsMenuScreen>( *display, 
                               "Reading Settings");
            for (int i = 0; i < fontsList.size(); i++) {
                fontTypeMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(fontsList[i].caption, [this, fontTypeMenu, i]() {
                    mainFont.load((MemoryPtr)fontsList[i].content, fontsList[i].length);
                    prefs_.fontType.set(i);
                    this->display_->clearDisplay();
                    fontTypeMenu->setNeedsRerender();
                }));
            }
            setCurrentScreen(fontTypeMenu);
            fontTypeMenu->setBackCallback([this]() {
               screenStack_.pop();
            });
        }));
    }
#endif

#if 0
    experimentalSettingsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Fonts Library",
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

    HEAP_PROFILE_END(rootUISettings);

    screenStack_.push(newMainMenu);

    if (prefs_.cesMode.get().value_or(false)) {
        auto cesScreen = std::make_shared<DemoScreen>(*display, prefs_);

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
            cesScreen->setBackCallback([this, display]() {
                display->clearDisplay();
                log_i("Setting Readable Screen after focus in CES mode");
                screenStack_.pop();
            });
            this->setCurrentScreen(cesScreen);
        } else {
            auto errorPopup = std::make_shared<MsgPopup>(*display, itemErrorMsg_);
            this->setCurrentScreen(errorPopup);
            errorPopup->setBackCallback([this]() {
                display_->clearDisplay();
                screenStack_.pop();
            });
        }
    } else if (prefs_.emcMode.get().value_or(false)) {
        log_d("Starting EMC mode readable reading turn page every 30s and content sync every "
              "60s");
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
            this->display_->setFont(static_cast<Font *>(&fontFace0));
            this->setCurrentScreen(readableScreen);
        } else {
            auto errorPopup = std::make_shared<MsgPopup>(*display, itemErrorMsg_);
            this->setCurrentScreen(errorPopup);
            errorPopup->setBackCallback([this]() {
                display_->clearDisplay();
                screenStack_.pop();
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
                this->display_->setFont(static_cast<Font *>(&fontFace0));
                this->setCurrentScreen(readableScreen);
                log_i("Resuming readable %s at place %s", (*it)->title.c_str(),
                      (*it)->place->str().c_str());
                // Technically. we might want this to go to Recent, but for now this will do.
                if (articlesIterator != articles.end()) {
                    newMainMenu->setTabToTitle("Articles");
                } else if (isBible(*it)) {
                    newMainMenu->setTabToTitle("Bible");
                } else {
                    newMainMenu->setTabToTitle("Books");
                }
            } else {
                auto errorPopup = std::make_shared<MsgPopup>(*display, itemErrorMsg_);
                this->setCurrentScreen(errorPopup);
                errorPopup->setBackCallback([this]() {
                    display_->clearDisplay();
                    screenStack_.pop();
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
        newMainMenu->setTabToTitle("Settings");
    }

    SHOW_HEAP();
}

void RootUI::handleBrowse(FileSystem &fileSystem,
                          const std::shared_ptr<GenericSettingsMenuScreen> &fsMenu,
                          const std::string &path) {
    FileList localFiles, localDirs;

    fileSystem.listDirWithSubdirectories(path, localFiles, localDirs);

    //    fsMenu->resetMenuItems();

    for (const std::string &dir : localDirs) {
        log_d("Next Dir is %s", dir.c_str());
        fsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
            StringFormat("%s/", dir.c_str()), [this, fsMenu, &fileSystem, path, dir]() {
                this->display_->clearDisplay();
                handleBrowse(fileSystem, fsMenu, path + dir + "/");
                // fsMenu->setExtraText(StringFormat("Browsing %s", (path + dir + "/").c_str()));
                fsMenu->setNeedsRerender();
            }));
    }

    for (const std::string &file : localFiles) {
        log_d("Next File is %s", file.c_str());
        fsMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
            file, [this, fsMenu, &fileSystem, path, file]() {
                SFile toSerialOutput = fileSystem.open(path + file);
                while (toSerialOutput.available()) {
                    printf("%c", toSerialOutput.read());
                }
                this->display_->clearDisplay();
                // fsMenu->setExtraText(StringFormat("output %s to serial", (path + file).c_str()));
                fsMenu->setNeedsRerender();
            }));
    }
}
