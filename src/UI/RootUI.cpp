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
#include "UI/Menu/DRMFreeWelcomeMenuScreen.hpp"
#include "UI/Menu/FirstBookWelcomeMenuScreen.hpp"
#include "UI/Menu/GenericSettingsMenuScreen.hpp"
#include "UI/Menu/InactivitySettingMenuScreen.hpp"
#include "UI/Menu/LetsFocusWelcomeMenuScreen.hpp"
#include "UI/Menu/MainMenuScreen.hpp"
#include "UI/Menu/MenuItemArticle.hpp"
#include "UI/Menu/MenuItemBook.hpp"
#include "UI/Menu/MenuItemCheckable.hpp"
#include "UI/Menu/MenuItemMultiLine.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/Menu/ReadingSettingsMenuScreen.hpp"
#include "UI/Menu/TextPlacementWelcomeMenuScreen.hpp"
#include "UI/Menu/WiFiSettingsMenuScreen.hpp"
#include "UI/MsgPopup.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/ReleaseNotesScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
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

extern RootUI rootUi;

auto RootUI::getInstance() -> RootUI & { return rootUi; }

void RootUI::pushScreen(const std::shared_ptr<Screen> &screen, bool deferRender) {
    screen->setNeedsRerender();
    screenStack_.push(screen);
    // If this was called from `loop()` then we'll rerender automatically at the end of the loop.
    // Otherwise, we should rerender now.
    if (!loopRunning_ && !deferRender) {
        log_d("Pushing screen outside loop() manually set, rerendering now");
        renderIfNeeded();
    }
}

void RootUI::popScreen(bool clearDisplay) {
    if (clearDisplay) {
        display_.clearDisplay();
    }
    if (!screenStack_.empty()) {
        screenStack_.pop();
    } else {
        log_e("Trying to pop screen with no screen to pop");
    }
    // If this was called from `loop()` then we'll rerender automatically at the end of the loop.
    // Otherwise, we should rerender now.
    if (!loopRunning_) {
        renderIfNeeded();
    }
}

void RootUI::popToMainMenu() {
    while (!screenStack_.empty() && !dynamic_cast<MainMenuScreen *>(screenStack_.top().get())) {
        popScreen(true);
    }
}

auto RootUI::needsRerender() -> bool {
    if (!screenStack_.empty()) {
        return currentScreen()->needsRerender();
    } else {
        return Screen::needsRerender();
    }
};

void RootUI::render(DisplaySystem &display) {
    if (!screenStack_.empty()) {
        currentScreen()->render(display);
    }
};

void RootUI::processCommand(RemoteCommand command) {
    if (!screenStack_.empty()) {
        currentScreen()->processCommand(command);
    }
}

void RootUI::clearNeedsRerender() {
    if (!screenStack_.empty()) {
        currentScreen()->clearNeedsRerender();
    } else {
        Screen::clearNeedsRerender();
    }
}

void RootUI::showTemporaryMessage(const std::string &msg, const std::function<void()> &cleanupFunc,
                                  int duration) {
    auto msgPopup = std::make_shared<MsgPopup>(display_, msg);
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    msgPopup->setBackCallback([this, cleanupFunc]() {
        // We do this because we want to ensure that the cleanup function is called after the screen
        // is popped. A little unintuitive, but it's what the interface needs (see
        // popAndSetIndexOrDone -- if we did that first, there'd be an extra pop).
        popScreen();
        cleanupFunc();
    });
    pushScreen(msgPopup);
    if (duration > 0) {
        msgPopup->startTimer(duration);
    }
}

// NOLINTNEXTLINE(readability-function-size): It's big, I know
void RootUI::setup(LocalStore &localStore, FileSystem &fileSystem,
                   LightingController &lightingController) {

    auto lastMagReading = power_.getLastMagReading();

    HEAP_PROFILE_START(rootUILogBootData);

    std::unordered_map<std::string, TelemetryVariant> bootData = {
        {"fw", std::string(SemanticVersionString())},
        {"sha", std::string(GitVersionString())},
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

    HEAP_PROFILE_START(MainMenuScreen);
    auto newMainMenu = std::make_shared<MainMenuScreen>(display_, power_.battery);
    HEAP_PROFILE_END(MainMenuScreen);

    SIZE_OF(*newMainMenu);

    auto createReadableScreen =
        [this, &fileSystem, &lightingController, &localStore](
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

        Font *font = FontFromPrefs(prefs_.font.get().value_or(PREFS_SOL_SMALL));

        auto readableScreen = std::make_shared<ReadableScreen>(
            stream, display_, readable, this->prefs_, this->telemetry_, fileSystem,
            lightingController, localStore, font);

        this->telemetry_.logMetric(TelemetryField::METRIC_READABLE_OPEN, readable->uuid,
                                   readable->telemetryPlacemarkMetrics());

        // When switching to a readable from a menu, we should clear the display
        // to make the whites really white and the blacks really black.
        display_.clearDisplay();
        readableScreen->setBackCallback([this, stream, readable]() {
            bool archived = prefs_.readableArchivedChanged.get(readable->shortenedUuid.c_str());
            auto title =
                StringFormat("%s%s", archived ? "Archived: " : "", readable->title.c_str());
            auto displayedTitle =
                StringFormat("%s%s", !archived ? "Archived: " : "", readable->title.c_str());
            log_d("Changing readable title to %s", title.c_str());

            // When leaving a readable to go back to the main menu, do a full clear
            // of the display.
            prefs_.lastReadableId.remove();
            this->popScreen();
            std::static_pointer_cast<MainMenuScreen>(currentScreen())->regenerateCurrentTab();
        });

        return readableScreen;
    };

    auto createTutorialScreen = [this](SpiramString text,
                                       int bit) -> std::shared_ptr<TutorialScreen> {
        auto tutorialScreen =
            std::make_shared<TutorialScreen>(display_, this->prefs_, std::move(text), bit);

        tutorialScreen->setBackCallback([this]() {
            this->popScreen();
            std::static_pointer_cast<MainMenuScreen>(currentScreen())->regenerateCurrentTab();
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
    auto makeReleaseNotesMenuItem = [this, &localStore](const Firmware &firmware) {
        log_i("Found pending firmware update %s", firmware.version.c_str());
        return std::make_shared<MenuItemMultiLine>(
            StringFormat("Upgrade Software from %s to %s", SemanticVersionString(),
                         firmware.version.c_str()),
            2, [this, &localStore, firmware]() {
                auto releaseNotesScreen = std::make_shared<ReleaseNotesScreen>(
                    display_, this->prefs_, power_, localStore, firmware);
                display_.clearDisplay();
                pushScreen(releaseNotesScreen);
                releaseNotesScreen->setBackCallback([this]() { this->popScreen(); });
            });
    };

    auto makeAndStartContentSync = [this, &localStore]() {
        auto contentSyncScreen = std::make_shared<ContentSyncScreen>(
            display_, localStore, this->prefs_, this->telemetry_, power_);
        contentSyncScreen->setBackCallback([this]() {
            // Ensure this sticks around until the end of the function
            // It wasn't picked up in the UIInnerLoop version because there isn't
            // a command sent to process the backCallback
            auto temp = currentScreen();
            // Dispatching to avoid temp going out of scope and the pointer getting cleaned up.
            DispatchGetUiQueue()->dispatch([this]() { this->popScreen(); });
            // TODO: After a successful content sync, re-setup the Local Store to be current.
            // Once we do that, the following two lines will regenerate the tab so we don't
            // need to restart to get the menu to be correct!
            // if (auto mainMenu = dynamic_cast<MainMenuScreen *>(currentScreen().get())) {
            //     mainMenu->regenerateCurrentTab();
            // }
        });
        this->pushScreen(contentSyncScreen);
        contentSyncScreen->begin();
    };

    auto makeContentSyncMenuItem =
        [this, makeAndStartContentSync](const char *title) -> std::shared_ptr<MenuItem> {
        return std::make_shared<MenuItemSingleLine>(title, [this, makeAndStartContentSync]() {
            // If we're not yet paired, show the app screen instead
            if (PreferencesStore::paired == PairedStatus::Unpaired) {
                auto appDownloadScreen =
                    std::make_shared<AppDownloadScreen>(display_, makeAndStartContentSync);
                this->pushScreen(appDownloadScreen);
                appDownloadScreen->setBackCallback([this]() { this->popScreen(); });
                return;
            }

            // If wifi & ble have been disabled, show a popup encouraging the user to
            // turn on wifi or BLE to perform the content sync.
            if (this->prefs_.wifiDisabled.get().value_or(false) &&
                this->prefs_.bleDisabled.get().value_or(false)) {
                showTemporaryMessage("Enable WiFi or BLE to perform a Content Sync.");
                currentScreen()->setFirstLoad();
                return;
            }

            makeAndStartContentSync();
        });
    };

    auto handleReadableScreenCreation =
        [this, createReadableScreen](const std::shared_ptr<Readable> &r) {
        auto readableScreen = createReadableScreen(r);
        if (readableScreen != nullptr) {
            this->pushScreen(readableScreen);
        } else {
            showTemporaryMessage(itemErrorMsg_, TEMP_MSG_DURATION_NO_CLOSE);
        }
    };

    SHOW_HEAP();

    log_i("Loading %zu Pocket articles", articles.size());

    auto welcomeIndex = [this, noContent]() -> int {
        if (!prefs_.seenLetsFocus.get().value_or(false)) {
            return 1;
        } else if (!prefs_.seenTextPlacement.get().value_or(false)) {
            return 2;
        } else if (noContent) {
            return 3;
        }
        return 4;
    };

    auto welcomeMenuFactory = [this, noContent, makeContentSyncMenuItem, &lightingController,
                               welcomeIndex, readables]() -> MenuItemSharedPtrVector {
        auto popAndSetIndexOrDone = [this, welcomeIndex]() {
            this->popToMainMenu();
            setFirstLoad();
            int welcomeTabIndex = welcomeIndex();
            if (welcomeTabIndex != 4) {
                std::static_pointer_cast<MainMenuScreen>(currentScreen())
                    ->setTabWithName("Welcome", welcomeTabIndex);
            } else {
                this->prefs_.dontShowWelcome.set(true);
                std::static_pointer_cast<MainMenuScreen>(currentScreen())->setCurrentTabIndex(1);
                showTemporaryMessage("Welcome Tab Complete!\nHappy Reading...");
            }
        };

        auto menuItems = std::make_shared<MenuItemVector>();

        menuItems->emplace_back(std::make_shared<MenuItemCheckable>(
            "Put on your Sol Reader!",
            [this, popAndSetIndexOrDone]() {
            auto manifestoScreen = std::make_shared<DemoScreen>(display_, prefs_);
            this->pushScreen(manifestoScreen);
            manifestoScreen->setBackCallback([this, popAndSetIndexOrDone]() {
                popAndSetIndexOrDone();
                if (needsRerender()) {
                    render(display_);
                    clearNeedsRerender();
                }
            });
            },
            []() {
            return true; /* return checked or not - this one is always checked */
        }));

        menuItems->emplace_back(std::make_shared<MenuItemCheckable>(
            "Let's focus (3 min)",
            [this, &lightingController, popAndSetIndexOrDone]() {
            auto letsFocusScreen = std::make_shared<LetsFocusWelcomeMenuScreen>(display_, prefs_);
            letsFocusScreen->addMenuItem(std::make_shared<MenuItemSingleLine>(
                "Back to Welcome Menu", [&lightingController, popAndSetIndexOrDone]() {
                    popAndSetIndexOrDone();
                    lightingController.resume();
                }));
            lightingController.pause();
            this->pushScreen(letsFocusScreen);
            },
            [this]() { return prefs_.seenLetsFocus.get().value_or(false); }));

        menuItems->emplace_back(std::make_shared<MenuItemCheckable>(
            "Text placement (2 min)",
            [this, popAndSetIndexOrDone]() {
            auto textPlacementScreen = std::make_shared<TextPlacementWelcomeMenuScreen>(display_);
            textPlacementScreen->addMenuItem(
                std::make_shared<MenuItemSingleLine>("Choose this template",
                                                     [this, popAndSetIndexOrDone]() {
                prefs_.seenTextPlacement.set(true);
                switch (std::static_pointer_cast<TextPlacementWelcomeMenuScreen>(currentScreen())
                            ->getCurrentTextSelection()) {
                case 0:
                    prefs_.textInsetLeft.set(35);
                    prefs_.textInsetRight.set(30);
                    prefs_.textInsetTop.set(25);
                    prefs_.textInsetBottom.set(25);
                    prefs_.lineSpacing.set(0);
                    prefs_.font.set(PREFS_SOL_SMALL);
                    log_d("Setting prefs to Comfortable");
                    showTemporaryMessage("Comfortable text placement set", popAndSetIndexOrDone);
                    break;
                case 1:
                    prefs_.textInsetLeft.set(25);
                    prefs_.textInsetRight.set(20);
                    prefs_.textInsetTop.set(15);
                    prefs_.textInsetBottom.set(15);
                    prefs_.font.set(PREFS_SOL_SMALL);
                    prefs_.lineSpacing.set(-2);
                    log_d("Setting prefs to Condensed");
                    showTemporaryMessage("Condensed text placement set", popAndSetIndexOrDone);
                    break;
                case 2:
                    prefs_.textInsetLeft.set(30);
                    prefs_.textInsetRight.set(20);
                    prefs_.textInsetTop.set(100);
                    prefs_.textInsetBottom.set(10);
                    prefs_.font.set(PREFS_SOL_MEDIUM);
                    prefs_.lineSpacing.set(0);
                    log_d("Setting prefs to Bottom");
                    showTemporaryMessage("Bottom text placement set", popAndSetIndexOrDone);
                    break;
                }
                }));
            textPlacementScreen->addMenuItem(std::make_shared<MenuItemSingleLine>(
                "Back to Welcome Menu", [this, popAndSetIndexOrDone]() {
                    // For brand new users, we want to make sure they actually select
                    // one of the text placement options.
                    // But for legacy users, if they have adjusted the text placement previously,
                    // and then cancel out of this screen, we don't want to force them to select
                    // one of the options just to have the checkmark appear. So this handles that
                    // case.
                    if (prefs_.textInsetLeft.get().has_value() ||
                        prefs_.textInsetRight.get().has_value() ||
                        prefs_.textInsetTop.get().has_value() ||
                        prefs_.textInsetBottom.get().has_value()) {
                        prefs_.seenTextPlacement.set(true);
                    }
                    popAndSetIndexOrDone();
                }));
            this->pushScreen(textPlacementScreen);
            },
            [this]() { return prefs_.seenTextPlacement.get().value_or(false); }));

        menuItems->emplace_back(std::make_shared<MenuItemCheckable>(
            noContent              ? "Get your first book (5 min)"
            : readables.size() > 1 ? StringFormat("You have %zu books", readables.size())
                                   : StringFormat("You have %zu book", readables.size()),
            [this, makeContentSyncMenuItem, popAndSetIndexOrDone]() {
            auto firstBookWelcomeMenuScreen =
                std::make_shared<FirstBookWelcomeMenuScreen>(display_);

            firstBookWelcomeMenuScreen->addMenuItem(
                makeContentSyncMenuItem("Get content via the Sol app"));

            firstBookWelcomeMenuScreen->addMenuItem(std::make_shared<MenuItemSingleLine>(
                "Direct transfer via USB", [this, popAndSetIndexOrDone]() {
                    auto drmFreeScreen = std::make_shared<DRMFreeWelcomeMenuScreen>(display_);
                    this->pushScreen(drmFreeScreen);
                    drmFreeScreen->setBackCallback(popAndSetIndexOrDone);
                    drmFreeScreen->addMenuItem(std::make_shared<MenuItemSingleLine>(
                        "Back", [this]() { this->popScreen(); }));
                }));

            firstBookWelcomeMenuScreen->addMenuItem(
                std::make_shared<MenuItemSingleLine>("Back to Welcome Menu", popAndSetIndexOrDone));

            this->pushScreen(firstBookWelcomeMenuScreen);
            },
            [noContent]() { return !noContent; }));

        return menuItems;
    };

    auto recentMenuFactory = [this, &localStore, handleReadableScreenCreation, isBible,
                              makeReleaseNotesMenuItem]() -> MenuItemSharedPtrVector {
        localStore.computeRecentReadables();
        auto &recents = localStore.recents();
        auto menuItems = std::make_shared<MenuItemVector>();

        if (this->prefs_.firmwareLoaded.get().has_value()) {
            Firmware firmware;
            auto upgradeAvailable = localStore.getPendingFirmwareUpdate(firmware);
            if (upgradeAvailable) {
                menuItems->emplace_back(makeReleaseNotesMenuItem(firmware));
            }
        }

        for (const auto &recent : recents) {
            bool archived = prefs_.readableArchivedChanged.get(recent->shortenedUuid.c_str());
            // Still needed for the Bible one since that's just a MenuItemSingleLine
            // The other two are handled by the menu item itself because they have the readable.
            auto title = StringFormat("%s%s", archived ? "Archived: " : "", recent->title.c_str());

            std::shared_ptr<MenuItem> menuItem;
            if (isBible(recent)) {
                menuItem = std::make_shared<MenuItemSingleLine>(
                    title, [recent, handleReadableScreenCreation]() {
                        handleReadableScreenCreation(recent);
                    });
            } else if (recent->isArticle()) {
                menuItem = std::make_shared<MenuItemArticle>(
                    recent,
                    [recent, handleReadableScreenCreation]() {
                    handleReadableScreenCreation(recent);
                    },
                    archived);
            } else {
                menuItem = std::make_shared<MenuItemBook>(
                    recent,
                    [recent, handleReadableScreenCreation]() {
                    handleReadableScreenCreation(recent);
                    },
                    archived);
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
            if (isBible(book)) {
                continue;
            }
            bool archived = prefs_.readableArchivedChanged.get(book->shortenedUuid.c_str());
            menuItems->emplace_back(std::make_shared<MenuItemBook>(
                book,
                [book, handleReadableScreenCreation]() { handleReadableScreenCreation(book); },
                archived));
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
                menuItems->emplace_back(std::make_shared<MenuItemBook>(
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
                std::make_shared<MenuItemSingleLine>("\ue000 Books Tutorial",
                                                     [this, createTutorialScreen]() {
                SpiramString message = SpiramString(
                    "Welcome to the Books menu. This tab "
                    "shows you the books currently synced to your Sol Reader. To add more books or "
                    "manage the books on your device, use the Sol app.\n\n"
                    "Did you know "
                    "you can change the position, width, and height of the page in the Sol Reader "
                    "display? "
                    "Simply go to Settings -> Reading -> Resize Page."
                    "\n\n\nPress "
                    "\ue000 (center) to continue.");
                auto tutorialScreen = createTutorialScreen(message, TUTORIAL_SCREEN_BOOKS);
                this->pushScreen(tutorialScreen);
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
            menuItems->emplace_back(std::make_shared<MenuItemArticle>(
                article,
                [article, handleReadableScreenCreation]() {
                handleReadableScreenCreation(article);
                },
                archived));
        }

        if (noArticles) {
            menuItems->emplace_back(makeContentSyncMenuItem("Sync Content"));
        }

        if (!(prefs_.tutorialsDeleted.get().value_or(0) & TUTORIAL_SCREEN_ARTICLES)) {
            menuItems->emplace_back(std::make_shared<MenuItemSingleLine>(
                "\ue000 Articles Tutorial", [this, createTutorialScreen]() {
                    SpiramString message = SpiRamStringFormat(
                        "Welcome to the Articles menu. This tab "
                        "shows you the articles currently synced to your Sol "
                        "Reader. To add more articles, "
                        "simply connect your Pocket account in the Sol app "
                        "or forward newsletters "
                        "from your inbox to add@solreader.com.\n\n"
                        "Did you know "
                        "you can change the sort order of your articles? "
                        "Simply go to Settings -> Content Management -> Article Sort Order."
                        "\n\n\nPress \ue000 (center) to "
                        "continue.\n");
                    auto tutorialScreen = createTutorialScreen(message, TUTORIAL_SCREEN_ARTICLES);
                    this->pushScreen(tutorialScreen);
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
                menuItems->emplace_back(std::make_shared<MenuItemSingleLine>(
                    std::string(title), [book, handleReadableScreenCreation]() {
                        handleReadableScreenCreation(book);
                    }));
            }
        }

        if (noContent) {
            std::shared_ptr<Readable> psalmsAndProverbs = std::make_shared<Readable>(
                "6863fd3c-77a9-468e-93fa-f70d8b79029f", "Psalms and Proverbs",
                PSALMSANDPROVERBS_EPUB, PSALMSANDPROVERBS_EPUB_SIZE);
            psalmsAndProverbs->authors.emplace_back("7632d6ec-7339-4752-8672-a966503e1003",
                                                    "Crossway ESV", "");
            if (!prefs_.readableArchivedChanged.get(psalmsAndProverbs->shortenedUuid.c_str())) {
                menuItems->emplace_back(std::make_shared<MenuItemSingleLine>(
                    std::string(psalmsAndProverbs->title),
                    [psalmsAndProverbs, handleReadableScreenCreation]() {
                    handleReadableScreenCreation(psalmsAndProverbs);
                    }));
            }
        }

        if ((!menuItems->empty()) &&
            (!(prefs_.tutorialsDeleted.get().value_or(0) & TUTORIAL_SCREEN_BIBLE))) {
            menuItems->emplace_back(std::make_shared<MenuItemSingleLine>(
                "\ue000 Why have the Bible?", [this, createTutorialScreen]() {
                    SpiramString message = SpiRamStringFormat(
                        "The Gutenberg Bible "
                        "was the earliest major book printed using mass-produced movable "
                        "metal type in Europe. It marked the start of the Gutenberg Revolution "
                        "and the age of printed books in the West. So we thought it would be "
                        "appropriate to include the Bible as one of the first books on the Sol "
                        "Reader.\n\n"
                        "Crossway Publishing has provided the English "
                        "Standard Version"
                        " of the Bible (Copyright 2001, Crossway Publishing).\n\n"
                        " Not your thing? No worries! Just archive these books and the tab "
                        " will disappear.\n\n"
                        " Press \ue000 (center) to continue.");
                    auto tutorialScreen = createTutorialScreen(message, TUTORIAL_SCREEN_BIBLE);
                    this->pushScreen(tutorialScreen);
                }));
        }
        return menuItems;
    };

    auto settingsMenuFactory = [this, &localStore, &fileSystem, hasLastQuestion,
                                makeContentSyncMenuItem,
                                &lightingController]() -> MenuItemSharedPtrVector {
        auto menuItems = std::make_shared<MenuItemVector>();

        menuItems->emplace_back(std::make_shared<MenuItemSingleLine>("Reading Style", [this]() {
            log_d("Loading Reading Settings Menu");

            auto readingSettingsMenu =
                std::make_shared<ReadingSettingsMenuScreen>(display_, "Main Menu", prefs_);

            if (this->prefs_.autoPilot.get().value_or(false)) {
                prefs_.autoPilotSpeed.set(10000);
                readingSettingsMenu->turnOnAutoPilot();
            } else {
                prefs_.autoPilotSpeed.remove();
                readingSettingsMenu->turnOffAutoPilot();
            }

            readingSettingsMenu->setBackCallback([this]() { this->popScreen(); });

            readingSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Text Size",
                                                                                  [this]() {
                auto fontMenu =
                    std::make_shared<ReadingSettingsMenuScreen>(display_, "Reading Style", prefs_);
                fontMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Small", [this]() {
                    display_.clearDisplay();
                    prefs_.font.set(PREFS_SOL_SMALL);
                    this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                   {{"text_size", std::string("small")}});
                    currentScreen()->setNeedsRerender();
                }));
                fontMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Medium", [this]() {
                    display_.clearDisplay();
                    prefs_.font.set(PREFS_SOL_MEDIUM);
                    this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                   {{"text_size", std::string("medium")}});
                    currentScreen()->setNeedsRerender();
                }));
                fontMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Large", [this]() {
                    display_.clearDisplay();
                    prefs_.font.set(PREFS_SOL_LARGE);
                    this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                   {{"text_size", std::string("large")}});
                    currentScreen()->setNeedsRerender();
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
                pushScreen(fontMenu);
                fontMenu->setBackCallback([this]() { this->popScreen(false); });
            }));

            readingSettingsMenu->addMenuItem(
                std::make_shared<MenuItemSingleLine>("Move Page in Display", [this]() {
                    log_d("Loading adjust offset");
                    auto offsetAdjust =
                        std::make_shared<OffsetAdjustScreen>(display_, prefs_, this->telemetry_);
                    this->pushScreen(offsetAdjust);
                    log_d("setting up callback");
                    offsetAdjust->setBackCallback([this]() { this->popScreen(false); });
                    log_d("set up callback");
                }));

            readingSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Resize Page",
                                                                                  [this]() {
                log_d("Loading resize box");
                auto resizeBox =
                    std::make_shared<ResizeBoxScreen>(display_, prefs_, this->telemetry_);
                this->pushScreen(resizeBox);
                log_d("setting up callback");
                resizeBox->setBackCallback([this]() { this->popScreen(false); });
                log_d("set up callback");
            }));

            readingSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Line Spacing",
                                                                                  [this]() {
                auto lineSpacingScreen =
                    std::make_shared<LineSpacingScreen>(display_, prefs_, this->telemetry_);
                this->pushScreen(lineSpacingScreen);
                lineSpacingScreen->setBackCallback([this]() {
                    this->popScreen(false);
                    std::static_pointer_cast<ReadingSettingsMenuScreen>(currentScreen())
                        ->setRendererLineSpacingFromPrefs();
                });
            }));

            readingSettingsMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                "Dark Mode",
                [this]() {
                this->prefs_.darkMode.set(!this->prefs_.darkMode.get().value_or(false));
                // Reserved for future use? Potentially lighting up and down?
                display_.flipInversion();
                // When we invert, we should do a full clear of the display to make sure
                // the blacks are really gone.
                display_.clearDisplay();
                prefs_.invertSetting.set(display_.getInversion());
                currentScreen()->setNeedsRerender();
                currentScreen()->setFirstLoad();
                },
                [this]() -> bool { return this->prefs_.darkMode.get().value_or(false); }));

            readingSettingsMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                "Autopilot",
                [this]() {
                this->prefs_.autoPilot.set(!this->prefs_.autoPilot.get().value_or(false));
                if (this->prefs_.autoPilot.get().value_or(false)) {
                    prefs_.autoPilotSpeed.set(10000);
                    // cast this to a readable
                    std::static_pointer_cast<ReadingSettingsMenuScreen>(currentScreen())
                        ->turnOnAutoPilot();
                } else {
                    prefs_.autoPilotSpeed.remove();
                    std::static_pointer_cast<ReadingSettingsMenuScreen>(currentScreen())
                        ->turnOffAutoPilot();
                }
                currentScreen()->setNeedsRerender();
                currentScreen()->setFirstLoad();
                },
                [this]() -> bool { return this->prefs_.autoPilot.get().value_or(false); }));

            readingSettingsMenu->setFirstLoad();
            this->pushScreen(readingSettingsMenu);
        }));

        menuItems->emplace_back(std::make_shared<MenuItemSingleLine>(
            "Content Management", [this, &localStore, makeContentSyncMenuItem]() {
                log_d("Loading Reading Settings Menu");

                auto contentSettingsMenu =
                    std::make_shared<GenericSettingsMenuScreen>(display_, "Main Menu");

                for (auto modelType : {ReadablesModelType::Book, ReadablesModelType::Article}) {
                    auto sortOrderMenuItem = std::make_shared<MenuItemSingleLine>(
                        modelType == ReadablesModelType::Book ? "Book Sort Order"
                                                              : "Article Sort Order",
                        [this, &localStore, modelType]() {
                    auto sortMenu =
                        std::make_shared<GenericSettingsMenuScreen>(display_, "Content Management");
                    sortMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                        "Alphabetical",
                        [this, &localStore, modelType]() {
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
                        currentScreen()->setNeedsRerender();
                        },
                        [this, modelType]() {
                        if (modelType == ReadablesModelType::Book) {
                            return (this->prefs_.bookSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::Alphabetical) &&
                                    this->prefs_.bookSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Ascending));
                        } else if (modelType == ReadablesModelType::Article) {

                            return (this->prefs_.articleSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::Alphabetical) &&
                                    this->prefs_.articleSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Ascending));
                        }
                        return false;
                    }));
                    sortMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                        "Date added, most recent",
                        [this, &localStore, modelType]() {
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
                        currentScreen()->setNeedsRerender();
                        },
                        [this, modelType]() {
                        if (modelType == ReadablesModelType::Book) {
                            return (this->prefs_.bookSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::DateModified) &&
                                    this->prefs_.bookSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Descending));
                        } else if (modelType == ReadablesModelType::Article) {

                            return (this->prefs_.articleSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::DateModified) &&
                                    this->prefs_.articleSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Descending));
                        }
                        return false;
                    }));
                    sortMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                        "Date added, least recent",
                        [this, &localStore, modelType]() {
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
                        currentScreen()->setNeedsRerender();
                        },
                        [this, modelType]() {
                        if (modelType == ReadablesModelType::Book) {
                            return (this->prefs_.bookSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::DateModified) &&
                                    this->prefs_.bookSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Ascending));
                        } else if (modelType == ReadablesModelType::Article) {

                            return (this->prefs_.articleSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::DateModified) &&
                                    this->prefs_.articleSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Ascending));
                        }
                        return false;
                    }));
                    sortMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                        "Last read, most recent",
                        [this, &localStore, modelType]() {
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
                        currentScreen()->setNeedsRerender();
                        },
                        [this, modelType]() {
                        if (modelType == ReadablesModelType::Book) {
                            return (this->prefs_.bookSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::LastRead) &&
                                    this->prefs_.bookSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Descending));
                        } else if (modelType == ReadablesModelType::Article) {

                            return (this->prefs_.articleSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::LastRead) &&
                                    this->prefs_.articleSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Descending));
                        }
                        return false;
                    }));
                    sortMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                        "Last read, least recent",
                        [this, &localStore, modelType]() {
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
                        currentScreen()->setNeedsRerender();
                        },
                        [this, modelType]() {
                        if (modelType == ReadablesModelType::Book) {
                            return (this->prefs_.bookSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::LastRead) &&
                                    this->prefs_.bookSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Ascending));
                        } else if (modelType == ReadablesModelType::Article) {

                            return (this->prefs_.articleSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::LastRead) &&
                                    this->prefs_.articleSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Ascending));
                        }
                        return false;
                    }));
                    sortMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                        "Random",
                        [this, &localStore, modelType]() {
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
                        currentScreen()->setNeedsRerender();
                        },
                        [this, modelType]() {
                        if (modelType == ReadablesModelType::Book) {
                            return (this->prefs_.bookSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::Random) &&
                                    this->prefs_.bookSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Ascending));
                        } else if (modelType == ReadablesModelType::Article) {

                            return (this->prefs_.articleSortCriteria.get().value_or(0) ==
                                        static_cast<int>(SortCriteria::Random) &&
                                    this->prefs_.articleSortDirection.get().value_or(0) ==
                                        static_cast<int>(SortDirection::Ascending));
                        }
                        return false;
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
                    pushScreen(sortMenu);
                    sortMenu->setBackCallback([this]() { this->popScreen(false); });
                        });
                    contentSettingsMenu->addMenuItem(sortOrderMenuItem);
                }

                contentSettingsMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                    "Show Welcome Tab",
                    [this]() {
                currentScreen()->setFirstLoad();
                if (this->prefs_.dontShowWelcome.get().value_or(false) &&
                    !this->prefs_.forceShowWelcome.get().value_or(false)) {
                    this->prefs_.dontShowWelcome.set(false);
                    this->prefs_.forceShowWelcome.set(true);
                    showTemporaryMessage("Welcome Tab will be\nshown on next restart.");

                } else {
                    this->prefs_.dontShowWelcome.set(true);
                    this->prefs_.forceShowWelcome.set(false);
                    showTemporaryMessage("Welcome Tab will be\nhidden on next restart.");
                }
                    },
                    [this]() -> bool {
                        return !this->prefs_.dontShowWelcome.get().value_or(false) ||
                               this->prefs_.forceShowWelcome.get().value_or(false);
                    }));

                contentSettingsMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("Sol Manifesto (1 min)", [this]() {
                        auto manifestoScreen = std::make_shared<DemoScreen>(display_, prefs_, true);
                        this->pushScreen(manifestoScreen);
                        manifestoScreen->setBackCallback([this]() {
                            this->popScreen();
                            if (needsRerender()) {
                                render(display_);
                                clearNeedsRerender();
                            }
                        });
                    }));

                contentSettingsMenu->addMenuItem(makeContentSyncMenuItem("Sync Content Now"));

                contentSettingsMenu->setBackCallback([this]() { this->popScreen(); });

                contentSettingsMenu->setFirstLoad();
                this->pushScreen(contentSettingsMenu);
            }));

        menuItems->emplace_back(std::make_shared<MenuItemSingleLine>("Experimental Features",
                                                                     [this]() {
            log_d("Loading Experimental Settings Menu");
            auto experimentalSettingsMenu =
                std::make_shared<GenericSettingsMenuScreen>(display_, "Main Menu");

            experimentalSettingsMenu->setBackCallback([this]() { this->popScreen(); });

            experimentalSettingsMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                "Show Partial Summary",
                [this]() {
                if (!prefs_.seenPartialSummaryOverview.get().value_or(false)) {
                    std::string message =
                        "You know how when you binge watch a show (you'd never do that, would "
                        "you?), you often will get a \"Previously on\" overview "
                        "at the beginning of each episode? This feature gives you \"Previously "
                        "on\" "
                        "but for your reading! Any time you leave a reading "
                        "session in the middle of a chapter, we'll generate a partial chapter "
                        "summary for you (no spoilers) to "
                        "review on your next reading session.\n\n\n"
                        "Press any button to continue...";
                    showTemporaryMessage(message, TEMP_MSG_DURATION_NO_CLOSE);
                    prefs_.seenPartialSummaryOverview.set(true);
                } else {
                    this->prefs_.noShowPartialChapterSummary.set(
                        !this->prefs_.noShowPartialChapterSummary.get().value_or(false));
                    currentScreen()->setNeedsRerender();
                }
                },
                [this]() -> bool {
                    return !this->prefs_.noShowPartialChapterSummary.get().value_or(false);
                }));

            experimentalSettingsMenu->addMenuItem(std::make_shared<MenuItemCheckable>(
                "Fast Page Transitions",
                [this]() {
                if (!prefs_.seenFastPageTransitionsOverview.get().value_or(false)) {
                    std::string message =
                        "Have you noticed on some screen transitions, the Eink "
                        "displays of your Sol Reader flash black and then white? "
                        "This is called a full refresh. By default, all page "
                        "turns use full refresh. "
                        "The Fast Page Transitions "
                        "option speeds up your page turns "
                        "by only doing a full refresh when needed to ensure the display doesn't "
                        "have too many remnants of previous pages.\n\n"
                        "Press any button to continue...";
                    showTemporaryMessage(message, TEMP_MSG_DURATION_NO_CLOSE);
                    prefs_.seenFastPageTransitionsOverview.set(true);
                } else {
                    this->prefs_.fastTransitions.set(
                        !this->prefs_.fastTransitions.get().value_or(false));
                    currentScreen()->setNeedsRerender();
                }
                },
                [this]() -> bool { return this->prefs_.fastTransitions.get().value_or(false); }));

            experimentalSettingsMenu->setFirstLoad();
            pushScreen(experimentalSettingsMenu);
            if (!prefs_.seenExperimentalOverview.get().value_or(false)) {
                std::string message =
                    "Welcome to our Experimental Features! These are new features to make your "
                    "reading experience even more next level. \n\n"
                    "Have an idea for a new feature? Drop us a line at hello@solreader.com or "
                    "message us "
                    "in the "
                    "Support section of the Reader tab in your Sol app.\n\n"
                    "Press any button to continue...";
                showTemporaryMessage(message, TEMP_MSG_DURATION_NO_CLOSE);
                prefs_.seenExperimentalOverview.set(true);
            }
        }));

        menuItems->emplace_back(std::make_shared<MenuItemSingleLine>(
            "System",
            [this, &lightingController, &localStore, &fileSystem]() {
            log_d("Loading System Settings Menu");

            auto systemSettingsMenu =
                std::make_shared<GenericSettingsMenuScreen>(display_, "Main Menu");

            systemSettingsMenu->setBackCallback([this]() { this->popScreen(); });

            systemSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("About", [this]() {
                auto aboutScreen = std::make_shared<AboutScreen>(display_, prefs_, power_);
                this->pushScreen(aboutScreen);
                aboutScreen->setBackCallback([this]() { this->popScreen(); });
            }));

            // Brightness and contrast are going to move elsewhere
            systemSettingsMenu->addMenuItem(
                std::make_shared<MenuItemSingleLine>("Brightness", [this, &lightingController]() {
                    auto lightingAdjustScreen = std::make_shared<LightingAdjustScreen>(
                        display_, this->prefs_, lightingController, this->telemetry_);
                    display_.clearDisplay();
                    pushScreen(lightingAdjustScreen);
                    lightingAdjustScreen->setBackCallback([this]() { this->popScreen(false); });
                }));

#if CONFIG_DISPLAY_EINKET013TT1
            systemSettingsMenu->addMenuItem(
                std::make_shared<MenuItemSingleLine>("Contrast", [this, &lightingController]() {
                    auto contrastAdjustScreen = std::make_shared<ContrastAdjustScreen>(
                        display_, this->prefs_, lightingController, this->telemetry_);
                    display_.clearDisplay();
                    pushScreen(contrastAdjustScreen);
                    contrastAdjustScreen->setBackCallback([this]() { this->popScreen(false); });
                }));
#endif

            systemSettingsMenu->addMenuItem(
                std::make_shared<MenuItemSingleLine>("Legal & Regulatory", [this]() {
                    auto legalScreen = std::make_shared<LegalScreen>(display_, prefs_, power_);
                    this->pushScreen(legalScreen);
                    legalScreen->setBackCallback([this]() { this->popScreen(); });
                }));

            systemSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Firmware Update",
                                                                                 [this]() {
                // If we don't have WiFi configured, show the QR code instead
                if (!WiFi::getInstance().hasWiFiCredentials()) {
                    auto appDownloadScreen = std::make_shared<AppDownloadScreen>(display_);
                    this->pushScreen(appDownloadScreen);
                    appDownloadScreen->setBackCallback([this]() { this->popScreen(); });
                    return;
                }

                std::string turnOnWifiMsg = "Enable WiFi from Settings -> Wifi and "
                                            "Bluetooth to perform a Firmware Update";

                // If wifi has been disabled, show a popup encouraging the user to
                // turn on wifi to perform the firmware sync.
                if (this->prefs_.wifiDisabled.get().value_or(false)) {
                    showTemporaryMessage(turnOnWifiMsg);
                    currentScreen()->setFirstLoad();
                    return;
                }

                auto firmwareUpdateScreen =
                    std::make_shared<FirmwareUpdateScreen>(display_, telemetry_, prefs_);
                this->pushScreen(firmwareUpdateScreen);
                firmwareUpdateScreen->setBackCallback([this]() { this->popScreen(false); });
                firmwareUpdateScreen->begin();
            }));

            systemSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Inactivity Timer",
                                                                                 [this]() {
                auto inactivityMenu =
                    std::make_shared<InactivitySettingMenuScreen>(display_, "System Settings");

                auto updateExtraText = [this]() {
                    std::string message = std::string(SpiRamStringFormat(
                        "Sleep after %d minutes of inactivity.",
                        prefs_.inactivityMinutes.get().value_or(DEFAULT_INACTIVITY_MINUTES)));
                    std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                        ->setExtraText(message);
                };

                // This used to be in a lambda but that leaks memory passing around the
                // inactivityMenuItem as a shared pointer.
                inactivityMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("5 minutes", [this, updateExtraText]() {
                        display_.clearDisplay();
                        prefs_.inactivityMinutes.set(5);
                        updateExtraText();
                        std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                            ->requireRestart();
                        this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                       {{"inactivity_minutes", 5}});
                        std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));

                inactivityMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("10 minutes", [this, updateExtraText]() {
                        display_.clearDisplay();
                        prefs_.inactivityMinutes.set(10);
                        updateExtraText();
                        std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                            ->requireRestart();
                        this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                       {{"inactivity_minutes", 10}});
                        std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));

                inactivityMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("15 minutes", [this, updateExtraText]() {
                        display_.clearDisplay();
                        prefs_.inactivityMinutes.set(15);
                        updateExtraText();
                        std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                            ->requireRestart();
                        this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                       {{"inactivity_minutes", 15}});
                        std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));

                inactivityMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("20 minutes", [this, updateExtraText]() {
                        display_.clearDisplay();
                        prefs_.inactivityMinutes.set(20);
                        updateExtraText();
                        std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                            ->requireRestart();
                        this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                       {{"inactivity_minutes", 20}});
                        std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));

                inactivityMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("30 minutes", [this, updateExtraText]() {
                        display_.clearDisplay();
                        prefs_.inactivityMinutes.set(30);
                        updateExtraText();
                        std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                            ->requireRestart();
                        this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                                       {{"inactivity_minutes", 30}});
                        std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
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

                inactivityMenu->setSelectedIndex(menuIndex);
                pushScreen(inactivityMenu);
                // This must be done after the pushScreen since it assumes the currentScreen
                // is the inactivityMenu.
                updateExtraText();

                inactivityMenu->setBackCallback([this]() {
                    if (std::static_pointer_cast<InactivitySettingMenuScreen>(currentScreen())
                            ->getRequireRestart()) {
                        PreferencesStore::settingsOnRestart = true;
                        showTemporaryMessage(settingsRestartMsg_, [this]() { power_.reboot(); });
                        this->power_.reboot();
                    } else {
                        this->popScreen(false);
                    }
                });
            }));

            systemSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
                "Clear Content (restart)", [this, &localStore]() {
                    localStore.deleteReadables(false);
                    localStore.deleteArticles(false);
                    showTemporaryMessage("Content cleared. Restarting...",
                                         [this]() { power_.reboot(); });
                }));

            systemSettingsMenu->addMenuItem(
                std::make_shared<MenuItemSingleLine>("WiFi and Bluetooth",
                                                     [this]() {
                auto wifiSettingsMenuScreen = std::make_shared<WiFiSettingsMenuScreen>(display_);

                wifiSettingsMenuScreen->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("WiFi Scan", [this]() {
                        auto wiFiScanScreen =
                            std::make_shared<WiFiScanScreen>(display_, WiFi::getInstance());
                        wiFiScanScreen->begin();
                        this->pushScreen(wiFiScanScreen);
                        wiFiScanScreen->setBackCallback([this]() { this->popScreen(); });
                    }));

                wifiSettingsMenuScreen->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("Clear WiFi Settings", [this]() {
                        WiFi::getInstance().clearWiFiCredentials();
                        log_w("WiFi settings cleared.");
                        std::static_pointer_cast<WiFiSettingsMenuScreen>(currentScreen())
                            ->loadWifiSettings();
                        showTemporaryMessage("WiFi settings cleared.");
                        currentScreen()->setFirstLoad();
                    }));

                wifiSettingsMenuScreen->addMenuItem(std::make_shared<MenuItemCheckable>(
                    "WiFi is Enabled",
                    [this]() {
                    if (this->prefs_.wifiDisabled.get().value_or(false)) {
                        this->prefs_.wifiDisabled.set(false);
                    } else {
                        this->prefs_.wifiDisabled.set(true);
                    }
                    std::static_pointer_cast<WiFiSettingsMenuScreen>(currentScreen())
                        ->setNeedsRerender();
                    },
                    [this]() -> bool { return !this->prefs_.wifiDisabled.get().value_or(false); }));

                wifiSettingsMenuScreen->addMenuItem(std::make_shared<MenuItemCheckable>(
                    "Bluetooth Sync is Enabled",
                    [this]() {
                    if (this->prefs_.bleDisabled.get().value_or(false)) {
                        this->prefs_.bleDisabled.set(false);
                    } else {
                        this->prefs_.bleDisabled.set(true);
                    }
                    std::static_pointer_cast<WiFiSettingsMenuScreen>(currentScreen())
                        ->setNeedsRerender();
                    },
                    [this]() -> bool { return !this->prefs_.bleDisabled.get().value_or(false); }));

                wifiSettingsMenuScreen->setBackCallback([this]() { this->popScreen(); });

                wifiSettingsMenuScreen->setFirstLoad();
                pushScreen(wifiSettingsMenuScreen);
                }));

            systemSettingsMenu->addMenuItem(
                std::make_shared<MenuItemSingleLine>("Factory Defaults", [this, &fileSystem]() {
                    auto lineMenuScreen =
                        std::make_shared<LineMenuScreen>(this->prefs_, this->display_);
                    lineMenuScreen->addMenuItem(std::make_shared<MenuItemSingleLine>(
                        "Cancel", [this]() { this->popScreen(); }));
                    lineMenuScreen->addMenuItem(std::make_shared<MenuItemSingleLine>(
                        "Factory Defaults: I'm sure!", [this, &fileSystem]() {
                            SoftFactoryReset(fileSystem);
                            log_w("NVS and FS cleared. Restarting...");
                            showTemporaryMessage("Resetting Device! Restarting...",
                                                 [this]() { power_.reboot(); });
                        }));
                    pushScreen(lineMenuScreen);
                    setNeedsRerender();
                }));

            systemSettingsMenu->setFirstLoad();
            this->pushScreen(systemSettingsMenu);
            }));

        if (prefs_.showDebugMenu.get().value_or(false)) {

            menuItems->emplace_back(std::make_shared<MenuItemSingleLine>(
                "Debug",
                [this, hasLastQuestion, &localStore, &fileSystem]() {
                log_d("Loading Debug Menu");

                auto debugSettingsMenu =
                    std::make_shared<GenericSettingsMenuScreen>(display_, "Main Menu");

                debugSettingsMenu->setBackCallback([this]() { this->popScreen(); });

#if CONFIG_USE_TURN_TOUCH_REMOTE
                debugSettingsMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("Unpair PageTurner", [this]() {
                        prefs_.pairedTurnTouchAddress.remove();
                        prefs_.lastPageTurnerBuild.remove();
                        PreferencesStore::lastPageTurnerBattery = 0;
                        log_w("Unpaired PageTurner. Restarting...");
                        showTemporaryMessage("Unpaired PageTurner! Resetting...",
                                             [this]() { power_.reboot(); });
                    }));
#endif

                debugSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("About",
                                                                                    [this]() {
                    auto debugAboutScreen = std::make_shared<DebugAboutScreen>(display_);
                    debugAboutScreen->setBackCallback([this]() { this->popScreen(); });
                    this->pushScreen(debugAboutScreen);
                }));

                if (hasLastQuestion) {
                    debugSettingsMenu->addMenuItem(
                        std::make_shared<MenuItemSingleLine>("Turn on Demo Mode", [this]() {
                            log_d("Demo On!");
                            prefs_.cesMode.set(true);
                            showTemporaryMessage("Demo mode on! Resetting...",
                                                 [this]() { power_.reboot(); });
                        }));
                }

                debugSettingsMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("Reset Preferences", [this]() {
                        prefs_.clear();
                        log_w("Preferences cleared. Restarting...");
                        showTemporaryMessage("Preferences cleared. Restarting...",
                                             [this]() { power_.reboot(); });
                    }));

                if (prefs_.allowUsageWhileCharging.get().value_or(false)) {
                    debugSettingsMenu->addMenuItem(
                        std::make_shared<MenuItemSingleLine>("Disable while charging", [this]() {
                            prefs_.allowUsageWhileCharging.remove();
                            showTemporaryMessage("Disabling use while charging...",
                                                 [this]() { power_.reboot(); });
                        }));
                } else {
                    debugSettingsMenu->addMenuItem(
                        std::make_shared<MenuItemSingleLine>("Enable while charging", [this]() {
                            prefs_.allowUsageWhileCharging.set(true);
                            showTemporaryMessage("Enabling use while charging...",
                                                 [this]() { power_.reboot(); });
                        }));
                }

                debugSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
                    "Clear Books (restart)", [this, &localStore]() {
                        localStore.deleteReadables();
                        showTemporaryMessage("Books reset.", [this]() { power_.reboot(); });
                    }));

                debugSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
                    "Clear Articles (restart)", [this, &localStore]() {
                        localStore.deleteArticles();
                        showTemporaryMessage("Articles reset.", [this]() { power_.reboot(); });
                    }));

                debugSettingsMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("Clear Crashes (restart)",
                                                         [this, &fileSystem]() {
                    FileList coreDumpFiles;
                    fileSystem.listDir("/coredumps", coreDumpFiles);
                    for (auto &file : coreDumpFiles) {
                        log_i("Removing %s", file.c_str());
                        fileSystem.remove(StringFormat("%s%s", "/coredumps/", file.c_str()));
                    }
                    showTemporaryMessage("Crash Reports cleared.", [this]() { power_.reboot(); });
                    }));
                debugSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
                    "File System Browser", [this, &fileSystem]() {
                        auto fsMenu =
                            std::make_shared<GenericSettingsMenuScreen>(display_, "Debug Settings");

                        handleBrowse(fileSystem, fsMenu);

                        pushScreen(fsMenu);
                        fsMenu->setBackCallback([this]() { this->popScreen(false); });
                    }));

                debugSettingsMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("Batteries and Temp", [this]() {
                        auto batteryScreen =
                            std::make_shared<BatteryScreen>(display_, prefs_, power_.battery);
                        this->pushScreen(batteryScreen);
                        batteryScreen->setBackCallback([this]() { this->popScreen(false); });
                    }));

                debugSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Optics Test",
                                                                                    [this]() {
                    auto opticsTestScreen = std::make_shared<CalibrationScreen>();
                    opticsTestScreen->setOpticsTest();
                    display_.clearDisplay();
                    this->pushScreen(opticsTestScreen);
                    opticsTestScreen->setBackCallback([this]() { this->popScreen(false); });
                }));

                debugSettingsMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("Binocular Optics Test", [this]() {
                        auto opticsTestScreen = std::make_shared<TestAlignmentScreen>();
                        display_.clearDisplay();
                        this->pushScreen(opticsTestScreen);
                        opticsTestScreen->setBackCallback([this]() { this->popScreen(false); });
                    }));

                SHOW_HEAP();

                if (hasLastQuestion) {
                    debugSettingsMenu->addMenuItem(
                        std::make_shared<MenuItemSingleLine>("Turn on EMC Mode", [this]() {
                            log_d("EMC On!");
                            prefs_.emcMode.set(true);
                            showTemporaryMessage("EMC mode on! Resetting...",
                                                 [this]() { power_.reboot(); });
                        }));

                    debugSettingsMenu->addMenuItem(
                        std::make_shared<MenuItemSingleLine>("Turn off EMC Mode", [this]() {
                            log_d("EMC Off!");
                            prefs_.emcMode.remove();
                            showTemporaryMessage("EMC mode off! Resetting...",
                                                 [this]() { power_.reboot(); });
                        }));
                }
                /*
                    debugSettingsMenu->addMenuItem(
                        std::make_shared<MenuItemSingleLine>("Alignment Setup", [this,
                   &lightingController]() { auto alignmentScreen =
                   std::make_shared<AlignmentScreen>(display_, prefs_, lightingController);
                   this->pushScreen(alignmentScreen); alignmentScreen->setBackCallback([this]() {
                   this->popScreen();
                            });
                        }));
                */
#if CONFIG_DISPLAY_EINKET013TT1
                debugSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Set Vcom",
                                                                                    [this]() {
                    auto vcomMenu =
                        std::make_shared<GenericSettingsMenuScreen>(display_, "Debug Settings");
                    auto *driver = (dynamic_cast<DisplayEinkET013TT1 *>(&display_))->getDriver();
                    vcomMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("0V",
                                                                               [this, driver]() {
                        driver->setVcom(0);
                        prefs_.vcom.set(0);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("-1V",
                                                                               [this, driver]() {
                        driver->setVcom(-1000);
                        prefs_.vcom.set(-1000);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("-2V",
                                                                               [this, driver]() {
                        driver->setVcom(-2000);
                        prefs_.vcom.set(-2000);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("-3V",
                                                                               [this, driver]() {
                        driver->setVcom(-3000);
                        prefs_.vcom.set(-3000);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("-4V",
                                                                               [this, driver]() {
                        driver->setVcom(-4000);
                        prefs_.vcom.set(-4000);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("-5V",
                                                                               [this, driver]() {
                        driver->setVcom(-5000);
                        prefs_.vcom.set(-5000);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));
                    vcomMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("-5.6V",
                                                                               [this, driver]() {
                        driver->setVcom(-5600);
                        prefs_.vcom.set(-5600);
                        std::static_pointer_cast<GenericSettingsMenuScreen>(currentScreen())
                            ->setNeedsRerender();
                    }));
                    pushScreen(vcomMenu);
                    vcomMenu->setBackCallback([this]() { this->popScreen(false); });
                }));
#endif

                debugSettingsMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("Set WiFi Power", [this]() {
                        auto wifiPowerMenu =
                            std::make_shared<GenericSettingsMenuScreen>(display_, "Debug Settings");

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

                        auto confirmMessage = [this](wifi_power_t power) {
                            this->prefs_.wifiPower.set(power);
                            showTemporaryMessage("Power set");
                        };

                        wifiPowerMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
                            "Max (19.5dBm)", [confirmMessage]() {
                                WiFi::getInstance().setWiFiPower(WIFI_POWER_19_5dBm);
                                confirmMessage(WIFI_POWER_19_5dBm);
                            }));
                        wifiPowerMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
                            "High (16dBm)", [confirmMessage]() {
                                WiFi::getInstance().setWiFiPower(WIFI_POWER_16dBm);
                                confirmMessage(WIFI_POWER_16dBm);
                            }));
                        wifiPowerMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
                            "Medium (11dBm)", [confirmMessage]() {
                                WiFi::getInstance().setWiFiPower(WIFI_POWER_11dBm);
                                confirmMessage(WIFI_POWER_11dBm);
                            }));
                        wifiPowerMenu->addMenuItem(
                            std::make_shared<MenuItemSingleLine>("Low (5dBm)", [confirmMessage]() {
                                WiFi::getInstance().setWiFiPower(WIFI_POWER_5dBm);
                                confirmMessage(WIFI_POWER_5dBm);
                            }));
                        wifiPowerMenu->addMenuItem(
                            std::make_shared<MenuItemSingleLine>("Min (-1dBm)", [confirmMessage]() {
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

                        pushScreen(wifiPowerMenu);
                        wifiPowerMenu->setBackCallback([this]() { this->popScreen(false); });
                    }));

#if USE_BLE
                debugSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Set BLE Power",
                                                                                    [this]() {
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
                        std::make_shared<GenericSettingsMenuScreen>(display_, "Debug Settings");

                    auto powerToString = [](esp_power_level_t power) -> std::string {
                        auto it = blePowerOptions.find(power);
                        return it != blePowerOptions.end() ? it->second : "Unknown";
                    };

                    esp_power_level_t currentPower = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);

                    // TODO: Maybe add this
                    // blePowerMenu->setExtraText(powerToString(currentPower));

                    auto confirmMessage = [this, powerToString](esp_power_level_t power) {
                        esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, power);
                        if (err != ESP_OK) {
                            log_e("esp_ble_tx_power_set ESP_BLE_PWR_TYPE_DEFAULT err:%s",
                                  esp_err_to_name(err));
                        }
                        prefs_.blePower.set(power);
                        showTemporaryMessage(
                            StringFormat("Power set to %s", powerToString(power).c_str()));
                    };
                    for (const auto &powerItem : blePowerOptions) {
                        blePowerMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
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

                    pushScreen(blePowerMenu);
                    blePowerMenu->setBackCallback([this]() { this->popScreen(false); });
                }));
#endif

#if CONFIG_DISPLAY_EINKET013TT1
                debugSettingsMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("Set Vcom Precise", [this]() {
                        auto vcomPreciseScreen =
                            std::make_shared<VcomAdjustScreen>(display_, this->prefs_);
                        display_.clearDisplay();
                        pushScreen(vcomPreciseScreen);
                        vcomPreciseScreen->setBackCallback([this]() { this->popScreen(false); });
                    }));
#endif

                debugSettingsMenu->addMenuItem(
                    std::make_shared<MenuItemSingleLine>("Format FS", [this, &fileSystem]() {
                        log_w("Formatting...");
                        showTemporaryMessage("Formatting, this will take several minutes.",
                                             [this, &fileSystem]() {
                        fileSystem.format();
                        power_.reboot();
                        });
                    }));

                debugSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Power Off",
                                                                                    [this]() {
                    log_w("Powering off.");
                    showTemporaryMessage("Powering off.", [this]() { power_.powerOff(); });
                }));

                debugSettingsMenu->setFirstLoad();
                this->pushScreen(debugSettingsMenu);
                }));
        }

        return menuItems;
    };

    int welcomeTabIndex = welcomeIndex();
    if (welcomeTabIndex == 4) {
        this->prefs_.dontShowWelcome.set(true);
    }

    if (!prefs_.dontShowWelcome.get().value_or(false) ||
        prefs_.forceShowWelcome.get().value_or(false)) {
        newMainMenu->addTab("Welcome", welcomeMenuFactory, 6);
    }

    if (!localStore.recents().empty()) {
        newMainMenu->addTab("Recent", recentMenuFactory, 4);
    }
    if (!booksMenuFactory()->empty()) {
        newMainMenu->addTab("Books", booksMenuFactory, 4);
    }
    if (!articlesMenuFactory()->empty()) {
        newMainMenu->addTab("Articles", articlesMenuFactory, 4);
    }

    if (!bibleMenuFactory()->empty()) {
        newMainMenu->addTab("Bible", bibleMenuFactory, 4);
    }

    newMainMenu->addTab("Settings", settingsMenuFactory, 5);

    newMainMenu->populateMenuView();

    SHOW_HEAP();

    HEAP_PROFILE_END(rootUIContent);

    HEAP_PROFILE_START(rootUISettings);

#if 0
    if (prefs_.fontsLibrary.get().value_or(false)) {
        readingSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Font Type (Experimental)",
                                                  [this, readingSettingsMenu]() {
            auto fontTypeMenu =
                std::make_shared<GenericSettingsMenuScreen>( display_, 
                               "Reading Style");
            for (int i = 0; i < fontsList.size(); i++) {
                fontTypeMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(fontsList[i].caption, [this, fontTypeMenu, i]() {
                    mainFont.load((MemoryPtr)fontsList[i].content, fontsList[i].length);
                    prefs_.fontType.set(i);
                    display_.clearDisplay();
                    fontTypeMenu->setNeedsRerender();
                }));
            }
            pushScreen(fontTypeMenu);
            fontTypeMenu->setBackCallback([this]() {
               this->popScreen(false);
            });
        }));
    }
#endif

#if 0
    experimentalSettingsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Fonts Library",
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

    pushScreen(newMainMenu, true);

    if (prefs_.cesMode.get().value_or(false)) {
        auto cesScreen = std::make_shared<DemoScreen>(display_, prefs_);

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
            cesScreen->setBackCallback([this]() { this->popScreen(); });
            this->pushScreen(cesScreen, true);
        } else {
            showTemporaryMessage(itemErrorMsg_);
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
            display_.clearDisplay();
            this->pushScreen(readableScreen, true);
        } else {
            showTemporaryMessage(itemErrorMsg_, TEMP_MSG_DURATION_NO_CLOSE);
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
                this->pushScreen(readableScreen, true);
                log_i("Resuming readable %s at place %s", (*it)->title.c_str(),
                      (*it)->place->str().c_str());

                // Note that this is fragile in how the id is cacluated from MenuItemBook and
                // MenuItemArticle. If those change, this will break.
                if (newMainMenu->isMenuIDInTab("Recent", (*it)->shortenedUuid.c_str())) {
                    newMainMenu->setTabWithName("Recent", (*it)->shortenedUuid.c_str());
                } else if (articlesIterator != articles.end()) {
                    newMainMenu->setTabWithName("Articles", (*it)->shortenedUuid.c_str());
                } else if (isBible(*it)) {
                    newMainMenu->setTabWithName("Bible", (*it)->shortenedUuid.c_str());
                } else {
                    log_d("Iterator shortened string is %s", (*it)->shortenedUuid.c_str());
                    newMainMenu->setTabWithName("Books", (*it)->shortenedUuid.c_str());
                }
            } else {
                showTemporaryMessage(itemErrorMsg_, TEMP_MSG_DURATION_NO_CLOSE);
            }
        } else {
            // Assume this readable has gone away (or changed from id to uuid)
            prefs_.lastReadableId.remove();
        }
    }

    bool showWelcomeVideo = false;

    // If we are in onboarding mode, show the entry bumper first
    if (!prefs_.dontShowWelcome.get().value_or(false)) {
        showWelcomeVideo = true;
        bool forceShowWelcome = prefs_.forceShowWelcome.get().value_or(false);

        if (welcomeTabIndex != 4 || forceShowWelcome) {
            newMainMenu->setTabWithName("Welcome", forceShowWelcome ? 0 : welcomeTabIndex);
            // If we're always showing the welcome tab, or if we have a current readable we're about
            // to open, don't show the video
            if (forceShowWelcome || currentReadable.has_value()) {
                showWelcomeVideo = false;
            }
        } else {
            // Onboaring is actually complete, so show the next menu over
            // instead of the welcome tab.
            newMainMenu->setCurrentTabIndex(1);
            showWelcomeVideo = false;
        }

        if (showWelcomeVideo) {
            auto firstTimeScreen = std::make_shared<DemoScreen>(display_, prefs_);
            pushScreen(firstTimeScreen, true);
            firstTimeScreen->setBackCallback([this]() {
                popScreen();
                // Any screen that could possibly get to a backCallback without a command
                // being sent through the loop must explicitly render at the end of its
                // backCallback to ensure the screen is properly redrawn.
                if (needsRerender()) {
                    render(display_);
                    clearNeedsRerender();
                }
            });
        }
    }

    if (!showWelcomeVideo && power_.battery.batteryVeryLow()) {
        showTemporaryMessage("Your battery is very low. Please charge your Sol Reader soon!", 4000);
    }

    // A settings changed caused this restart, so go to the settings tab
    // and clear out that pref.
    if (PreferencesStore::settingsOnRestart) {
        PreferencesStore::settingsOnRestart = false;
        newMainMenu->setTabWithName("Settings");
    }

    renderIfNeeded();
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
        fsMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
            StringFormat("%s/", dir.c_str()), [this, fsMenu, &fileSystem, path, dir]() {
                display_.clearDisplay();
                handleBrowse(fileSystem, fsMenu, path + dir + "/");
                // fsMenu->setExtraText(StringFormat("Browsing %s", (path + dir +
                // "/").c_str()));
                fsMenu->setNeedsRerender();
            }));
    }

    for (const std::string &file : localFiles) {
        log_d("Next File is %s", file.c_str());
        fsMenu->addMenuItem(
            std::make_shared<MenuItemSingleLine>(file, [this, fsMenu, &fileSystem, path, file]() {
                SFile toSerialOutput = fileSystem.open(path + file);
                while (toSerialOutput.available()) {
                    printf("%c", toSerialOutput.read());
                }
                display_.clearDisplay();
                // fsMenu->setExtraText(StringFormat("output %s to serial", (path +
                // file).c_str()));
                fsMenu->setNeedsRerender();
            }));
    }
}
