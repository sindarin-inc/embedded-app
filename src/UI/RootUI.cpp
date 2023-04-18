#include "RootUI.hpp"

#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <filesystem>
#include <nvs_flash.h>

#include "Bitmaps/Logo.hpp"
#include "Communications/HtmlCleaner.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "EPub/EPubFile.hpp"
#include "GitVersion.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/LookBackStream.hpp"
#include "Misc/MenuStyle.hpp"
#include "Misc/Platform.hpp"
#include "Misc/SolClock.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/LocalStore.hpp"
#include "UI/AlignmentScreen.hpp"
#include "UI/BatteryScreen.hpp"
#include "UI/CalibrationScreen.hpp"
#include "UI/CesScreen.hpp"
#include "UI/ContentSyncScreen.hpp"
#include "UI/CrashReportScreen.hpp"
#include "UI/DemoScreen.hpp"
#include "UI/FirmwareUpdateScreen.hpp"
#include "UI/LayoutTestScreen.hpp"
#include "UI/LightingAdjustScreen.hpp"
#include "UI/LineMenuScreen.hpp"
#include "UI/LineSpacingScreen.hpp"
#include "UI/LogoScreen.hpp"
#include "UI/MainMenuScreen.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/ReleaseNotesScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/StatsScreen.hpp"
#include "UI/ToggleMenuScreen.hpp"
#include "UI/VoltageLogUploadScreen.hpp"
#include "Version.hpp"
#include "config.h"
#include "sindarin-debug.h"

#if USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
#endif

#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#include "UI/VcomAdjustScreen.hpp"
#endif

#define LAST_READABLE_ID "rui-readable"

void RootUI::setup(DisplaySystem *display, LocalStore &localStore, FileSystem &fileSystem,
                   LightingController &lightingController) {
    SHOW_HEAP();

    display_ = display;

    MenuStyle styleFamilies;

    auto mainMenu = new MainMenuScreen(power_.battery, *display_, styleFamilies.mainMenu);

    SIZE_OF(*mainMenu);

    auto readables = localStore.books();
    auto articles = localStore.articles();

    auto createReadableScreen = [this, display, mainMenu, &fileSystem, &lightingController,
                                 &localStore](const Readable &readable) -> ReadableScreen * {
        // TODO: Would be better to not allocate / delete these all the time

        std::shared_ptr<RendererStream> stream;
        auto path = localStore.readableFilePathForReading(readable);

        if (HasSuffix(path, ".epub")) {
            log_i("Opening %s as epub", path.c_str());
            auto epubFile = std::make_unique<EPubFile>(fileSystem, path.c_str());
            epubFile->open();
            stream = std::make_shared<LookBackStream>(
                std::make_unique<HtmlCleaner>(std::move(epubFile)), LOOKBACK_BUFFER_SIZE);
        } else if (HasSuffix(path, ".txt.gz")) {
            log_i("Opening %s as gzip", path.c_str());
            stream = fileSystem.newGzipRendererStream(path.c_str());
        } else {
            stream = fileSystem.newRendererStream(path.c_str());
        }
        auto *readableScreen = new ReadableScreen(stream, *display, readable, this->prefs_,
                                                  power_.battery, fileSystem, lightingController);

        // When switching to a readable from a menu, we should clear the display
        // to make the whites really white and the blacks really black.
        display->clearDisplay();
        readableScreen->backCallback = [this, display, stream, readableScreen, mainMenu]() {
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
        };
        return readableScreen;
    };

    auto currentReadable = prefs_.lastReadableId.get();

    // All we need to do at this level is to have a mode where the glasses have full LED on
    // and a mode where a page turns every 30 seconds.
    //
    // Currently, the wifi plan is to have the Espressif FCC test mode take care of that
    // piece. But if it doesn't, then we can add it here by doing a wifi connect,
    // sync content, delete content, repeat.
    if (prefs_.emcMode.get().value_or(false)) {
        mainMenu->addMenuItem(MenuItem("Mode 1 On", []() {}));
        mainMenu->addMenuItem(
            MenuItem("Mode 2 (reading)", [this, display, readables, createReadableScreen]() {
                log_d("Starting EMC mode readable reading turn page every 30 seconds");
                auto it = std::find_if(readables.begin(), readables.end(), [](const Readable &x) {
                    return (x.title.compare("The Last Question") == 0);
                });
                if (it == readables.end()) {
                    log_e("Could not find readable: %s", "The Last Question");
                } else {
                    log_d("Found readable: %s", (*it).title.c_str());
                }
                auto readableScreen = createReadableScreen(*it);
                display->clearDisplay();
                this->display_->setFont(this->display_->style.getFontFamily().readableFont);
                this->setCurrentScreen(*readableScreen);
            }));
    }

    ReadableScreen *psalmsProverbs = nullptr;
    ReadableScreen *newTestament = nullptr;
    ReadableScreen *oldTestament = nullptr;

    // Bible hack (to show off if we wanted to have different "apps" that had different behavior.
    // For now, it's just the same as a regular book.)
    auto it = std::find_if(readables.begin(), readables.end(),
                           [](const Readable &x) { return (x.title == "Psalms and Proverbs"); });
    if (it == readables.end()) {
        log_d("Could not find readable: %s", "Psalms and Proverbs");
    } else {
        psalmsProverbs = createReadableScreen(*it);
        log_d("Found readable: %s", (*it).title.c_str());
    }

    it = std::find_if(readables.begin(), readables.end(),
                      [](const Readable &x) { return (x.title.compare("New Testament") == 0); });
    if (it == readables.end()) {
        log_d("Could not find readable: %s", "New Testament");
    } else {
        newTestament = createReadableScreen(*it);
        log_d("Found readable: %s", (*it).title.c_str());
    }

    it = std::find_if(readables.begin(), readables.end(),
                      [](const Readable &x) { return (x.title.compare("Old Testament") == 0); });
    if (it == readables.end()) {
        log_d("Could not find readable: %s", "Old Testament");
    } else {
        oldTestament = createReadableScreen(*it);
        log_d("Found readable: %s", (*it).title.c_str());
    }

    bool hasLastQuestion = false;
    it = std::find_if(readables.begin(), readables.end(), [](const Readable &x) {
        return (x.title.compare("The Last Question") == 0);
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

    auto booksMenu = new Menu(mainMenu->getMyStyle(), "Books");

    // If there is firmware available, show the Update Firmware item in the books tab.
    auto makeReleaseNotesMenuItem = [this, &localStore, display, mainMenu]() {
        Firmware firmware;
        localStore.getPendingFirmwareUpdate(firmware);
        log_i("Found pending firmware update %s", firmware.version.c_str());
        return MenuItem(
            "Upgrade Software",
            StringFormat("From %s to %s", kVersionString, firmware.version.c_str()),
            [this, display, mainMenu, &localStore, firmware]() {
                auto releaseNotesScreen =
                    new ReleaseNotesScreen(*display, this->prefs_, power_, localStore, firmware);
                display->clearDisplay();
                this->setCurrentScreen(*releaseNotesScreen);
                // auto resizeBox = new ResizeBoxScreen(*display, prefs_);
                // this->setCurrentScreen(*resizeBox);
                releaseNotesScreen->backCallback = [this, display, releaseNotesScreen, mainMenu]() {
                    display->clearDisplay();
                    mainMenu->setFirstLoad();
                    setCurrentScreen(*mainMenu);
                    delete releaseNotesScreen;
                };
            });
    };

    if (prefs_.firmwareLoaded.get().has_value()) {
        booksMenu->addMenuItem(makeReleaseNotesMenuItem());
    }

    // If there are no books, show the Sync Content item in the books tab.
    if (readables.size() == 0) {
        booksMenu->addMenuItem(
            MenuItem("Sync Content", [this, display, &localStore, mainMenu, &lightingController]() {
                auto contentSyncScreen = new ContentSyncScreen(*display, localStore, this->prefs_);
                lightingController.setOverrideValuePercent(lightingController.getCurrentPercent());
                display->setFont(mainMenu->getMyStyle().navigationFont);
                this->setCurrentScreen(*contentSyncScreen);
                contentSyncScreen->backCallback = [this, contentSyncScreen, mainMenu, display]() {
                    display->clearDisplay();
                    mainMenu->setFirstLoad();
                    setCurrentScreen(*mainMenu);
                    mainMenu->render(*display);
                    delete contentSyncScreen;
                };
                contentSyncScreen->begin();
            }));
    }

    for (const Readable &r : readables) {
        bool archived = prefs_.readableArchivedChanged.get(r.shortenedUuid.c_str());
        auto title = StringFormat("%s%s", archived ? "Archived: " : "", r.title.c_str());
        // Bible books go in their own menu / app
        if (r.title == "Psalms and Proverbs" || r.title == "New Testament" ||
            r.title == "Old Testament") {
            continue;
        }
        booksMenu->addMenuItem(MenuItem(StringFormat("%s", title.c_str()),
                                        StringFormat("%s", r.authors[0].name.c_str()),
                                        [this, r, createReadableScreen]() {
                                            auto readableScreen = createReadableScreen(r);
                                            this->setCurrentScreen(*readableScreen);
                                        }));
    }

    if (!prefs_.noBooks.get().value_or(false)) {
        mainMenu->addSubMenu(booksMenu);
    }

    SHOW_HEAP();

    log_i("Loading %zu Pocket articles", articles.size());

    auto pocketMenu = new Menu(mainMenu->getMyStyle(), "Articles");

    if (prefs_.firmwareLoaded.get().has_value()) {
        pocketMenu->addMenuItem(makeReleaseNotesMenuItem());
    }

    // If there are no articles, show the Sync Content item in the articles tab.
    if (articles.size() == 0) {
        pocketMenu->addMenuItem(
            MenuItem("Sync Content", [this, display, &localStore, mainMenu, &lightingController]() {
                auto contentSyncScreen = new ContentSyncScreen(*display, localStore, this->prefs_);
                lightingController.setOverrideValuePercent(lightingController.getCurrentPercent());
                display->setFont(mainMenu->getMyStyle().navigationFont);
                this->setCurrentScreen(*contentSyncScreen);
                contentSyncScreen->backCallback = [this, contentSyncScreen, mainMenu, display]() {
                    display->clearDisplay();
                    mainMenu->setFirstLoad();
                    setCurrentScreen(*mainMenu);
                    mainMenu->render(*display);
                    delete contentSyncScreen;
                };
                contentSyncScreen->begin();
            }));
    }

    for (const Readable &r : articles) {
        bool archived = prefs_.readableArchivedChanged.get(r.shortenedUuid.c_str());
        auto title = StringFormat("%s%s", archived ? "Archived: " : "", r.title.c_str());

        pocketMenu->addMenuItem(MenuItem(title, [this, display, pocketMenu, mainMenu, r,
                                                 createReadableScreen]() {
            display->clearDisplay();
            auto readableScreen = createReadableScreen(r);
            setCurrentScreen(*readableScreen);
            readableScreen->backCallback = [this, r, display, readableScreen, mainMenu,
                                            pocketMenu]() {
                bool archived = prefs_.readableArchivedChanged.get(r.shortenedUuid.c_str());
                auto title = StringFormat("%s%s", archived ? "Archived: " : "", r.title.c_str());
                pocketMenu->changeItemTitleAt(pocketMenu->getCurrSelection(), title);
                display->clearDisplay();
                mainMenu->setFirstLoad();
                setCurrentScreen(*mainMenu);
                delete readableScreen;
            };
        }));
    }

    if (!prefs_.noArticles.get().value_or(false)) {
        mainMenu->addSubMenu(pocketMenu);
    }

    if (!prefs_.noBible.get().value_or(false) && (oldTestament || newTestament || psalmsProverbs)) {
        auto bibleMenu = new Menu(mainMenu->getMyStyle(), "Bible");

        for (const Readable &r : readables) {
            bool archived = prefs_.readableArchivedChanged.get(r.shortenedUuid.c_str());
            auto title = StringFormat("%s%s", archived ? "Archived: " : "", r.title.c_str());
            if (r.title == "Psalms and Proverbs" || r.title == "New Testament" ||
                r.title == "Old Testament") {
                bibleMenu->addMenuItem(MenuItem(title, [this, r, createReadableScreen, display,
                                                        mainMenu]() {
                    auto readableScreen = createReadableScreen(r);
                    this->setCurrentScreen(*readableScreen);
                    readableScreen->backCallback = [this, r, display, readableScreen, mainMenu]() {
                        display->clearDisplay();
                        mainMenu->setFirstLoad();
                        setCurrentScreen(*mainMenu);
                        delete readableScreen;
                    };
                }));
            }
        }
        mainMenu->addSubMenu(bibleMenu);
    }

    auto settingsMenu = new Menu(styleFamilies.settingsMenu, "Settings");
    auto readingSettingsMenu =
        new MenuScreen(power_.battery, *display, styleFamilies.settingsMenu, "Reading");

    readingSettingsMenu->backCallback = [this, mainMenu]() {
        this->display_->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
    };

    auto contentSettingsMenu =
        new ToggleMenuScreen(power_.battery, *display_, styleFamilies.settingsMenu, "Content");

    contentSettingsMenu->backCallback = [this, mainMenu, display, contentSettingsMenu]() {
        // Ideally we'd move this into the ToggleClass so it's not duplicated with
        // experimentalSettingsMenu below, but that would require teh ToggleMenuScreen
        // to have the power intead of just the battery and also the prefs. Big deal or not?
        if (contentSettingsMenu->getRequireRestart()) {
            // Start on the settings menu when we boot so it feels like we just
            // went back to it!
            PreferencesStore::settingsOnRestart = true;
            contentSettingsMenu->showTemporaryMessage(
                *display, "Settings change required restart! Restarting...", 1500, false);
            this->power_.reboot();
        } else {
            this->display_->clearDisplay();
            mainMenu->setFirstLoad();
            setCurrentScreen(*mainMenu);
        }
    };

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

    contentSettingsMenu->addMenuItem(MenuItem("Deliver content on close", [this]() {
        this->prefs_.noDeliverOnClose.set(!this->prefs_.noDeliverOnClose.get().value_or(false));
    }));

    if (prefs_.noDeliverOnClose.get().value_or(false)) {
        contentSettingsMenu->setToggleLastItem(false);
    } else {
        contentSettingsMenu->setToggleLastItem(true);
    }

    contentSettingsMenu->addMenuItem(MenuItem("Morning delivery", [this]() {
        this->prefs_.morningDelivery.set(!this->prefs_.morningDelivery.get().value_or(false));
        if (this->prefs_.morningDelivery.get().value_or(false)) {
            // Default to 6am delivery. May have configuration at some other time
            this->prefs_.morningNewspaperTime.set(6);
        } else {
            this->prefs_.morningNewspaperTime.set(0);
        }
    }));

    if (prefs_.morningDelivery.get().value_or(false)) {
        contentSettingsMenu->setToggleLastItem(true);
    } else {
        contentSettingsMenu->setToggleLastItem(false);
    }

    contentSettingsMenu->addMenuItem(MenuItem("Evening delivery", [this]() {
        this->prefs_.eveningDelivery.set(!this->prefs_.eveningDelivery.get().value_or(false));
        if (this->prefs_.eveningDelivery.get().value_or(false)) {
            // default to 8pm delivery. May have configuration at some other time
            this->prefs_.eveningNewspaperTime.set(20);
        } else {
            this->prefs_.eveningNewspaperTime.set(0);
        }
    }));

    if (prefs_.eveningDelivery.get().value_or(false)) {
        contentSettingsMenu->setToggleLastItem(true);
    } else {
        contentSettingsMenu->setToggleLastItem(false);
    }

    auto systemSettingsMenu =
        new MenuScreen(power_.battery, *display, styleFamilies.settingsMenu, "System");

    systemSettingsMenu->backCallback = [this, mainMenu]() {
        this->display_->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
    };

    auto experimentalSettingsMenu =
        new ToggleMenuScreen(power_.battery, *display_, styleFamilies.settingsMenu, "Experimental");

    experimentalSettingsMenu->backCallback = [this, display, mainMenu, experimentalSettingsMenu]() {
        if (experimentalSettingsMenu->getRequireRestart()) {
            // Start on the settings menu when we boot so it feels like we just
            // went back to it!
            PreferencesStore::settingsOnRestart = true;
            experimentalSettingsMenu->showTemporaryMessage(
                *display, "Settings change required restart! Restarting...", 1500, false);
            this->power_.reboot();
        } else {
            this->display_->clearDisplay();
            mainMenu->setFirstLoad();
            setCurrentScreen(*mainMenu);
        }
    };

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

    experimentalSettingsMenu->addMenuItem(MenuItem("Autopilot", [this]() {
        this->prefs_.autoPilot.set(!this->prefs_.autoPilot.get().value_or(false));
        if (this->prefs_.autoPilot.get().value_or(false)) {
            prefs_.autoPilotSpeed.set(10000);
        } else {
            prefs_.autoPilotSpeed.set(0);
        }
    }));

    if (prefs_.autoPilot.get().value_or(false)) {
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

    experimentalSettingsMenu->addMenuItem(MenuItem("Upload Crashes on Sync", [this]() {
        this->prefs_.noUploadCrashes.set(!this->prefs_.noUploadCrashes.get().value_or(false));
    }));

    // This one is opposite of the above because we want to default it to true and make them
    // turn it off if they don't want it. At least for beta.
    if (prefs_.noUploadCrashes.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(false);
    } else {
        experimentalSettingsMenu->setToggleLastItem(true);
    }

    experimentalSettingsMenu->addMenuItem(
        MenuItem("Show Debug Menu", [this, experimentalSettingsMenu]() {
            this->prefs_.showDebugMenu.set(!this->prefs_.showDebugMenu.get().value_or(false));
            experimentalSettingsMenu->requireRestart();
        }));

    if (prefs_.showDebugMenu.get().value_or(false)) {
        experimentalSettingsMenu->setToggleLastItem(true);
    } else {
        experimentalSettingsMenu->setToggleLastItem(false);
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
    auto debugSettingsMenu =
        new MenuScreen(power_.battery, *display, styleFamilies.settingsMenu, "Debug");

    debugSettingsMenu->backCallback = [this, mainMenu]() {
        this->display_->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
    };

    // For items we don't want to throw away yet, but this isn't hooked up anywhere
    auto oldMenu = new MenuScreen(power_.battery, *display, styleFamilies.settingsMenu);

    // settingsMenu->addMenuItem(MenuItem("Layout Example", [this, settingsMenu, display]() {
    //    auto layoutTestScreen = new LayoutTestScreen(*display);
    //    setCurrentScreen(*layoutTestScreen);
    //    layoutTestScreen->backCallback = [this, layoutTestScreen, settingsMenu]() {
    //       settingsMenu->setFirstLoad();
    //        setCurrentScreen(*settingsMenu);
    //        delete layoutTestScreen;
    //    };
    // }));

    /*
        settingsMenu->addMenuItem(
            MenuItem("Sync Content", [this, display, &localStore, mainMenu, &lightingController]() {
                auto contentSyncScreen = new ContentSyncScreen(*display, localStore, this->prefs_);
                this->setCurrentScreen(*contentSyncScreen);
                lightingController.setOverrideValuePercent(lightingController.getCurrentPercent());
                contentSyncScreen->backCallback = [this, contentSyncScreen, mainMenu, display]() {
                    this->display_->clearDisplay();
                    mainMenu->setFirstLoad();
                    setCurrentScreen(*mainMenu);
                    mainMenu->render(*display);
                    delete contentSyncScreen;
                };
                contentSyncScreen->begin();
            }));
    */

    settingsMenu->addMenuItem(MenuItem("Reading", [this, readingSettingsMenu]() {
        log_d("Loading Reading Settings Menu");
        readingSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*readingSettingsMenu);
    }));

    settingsMenu->addMenuItem(MenuItem("Content", [this, contentSettingsMenu]() {
        log_d("Loading Content Settings Menu");
        contentSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*contentSettingsMenu);
    }));

    settingsMenu->addMenuItem(MenuItem("System", [this, systemSettingsMenu]() {
        log_d("Loading System Settings Menu");
        systemSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*systemSettingsMenu);
    }));

    settingsMenu->addMenuItem(MenuItem("Experimental", [this, experimentalSettingsMenu]() {
        log_d("Loading Content Settings Menu");
        experimentalSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*experimentalSettingsMenu);
    }));

    if (prefs_.showDebugMenu.get().value_or(false)) {
        settingsMenu->addMenuItem(MenuItem("Debug", [this, debugSettingsMenu]() {
            log_d("Loading Debug Settings Menu");
            debugSettingsMenu->setFirstLoad();
            this->setCurrentScreen(*debugSettingsMenu);
        }));
    }

    readingSettingsMenu->addMenuItem(MenuItem("Text Size", [this, display, readingSettingsMenu]() {
        auto fontMenu = new MenuScreen(power_.battery, *display, readingSettingsMenu->getMyStyle(),
                                       "Font Settings", "Reading Settings");
        fontMenu->setExtraText("Example text size");
        fontMenu->addMenuItem(MenuItem("Small", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.tahomaRegular);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_TAHOMA_REGULAR);
            fontMenu->setNeedsRerender();
        }));
        fontMenu->addMenuItem(MenuItem("Large", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.tahomaLarge);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_TAHOMA_LARGE);
            fontMenu->setNeedsRerender();
        }));
        setCurrentScreen(*fontMenu);
        fontMenu->backCallback = [this, fontMenu, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete fontMenu;
        };
    }));

    readingSettingsMenu->addMenuItem(MenuItem("Brightness", [this, display, readingSettingsMenu,
                                                             &lightingController]() {
        auto lightingAdjustScreen =
            new LightingAdjustScreen(*display, this->prefs_, lightingController);
        this->display_->clearDisplay();
        setCurrentScreen(*lightingAdjustScreen);
        lightingAdjustScreen->backCallback = [this, lightingAdjustScreen, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete lightingAdjustScreen;
        };
    }));

    readingSettingsMenu->addMenuItem(
        MenuItem("Move Page in Display", [this, readingSettingsMenu, display]() {
            log_d("Loading adjust offset");
            auto offsetAdjust = new OffsetAdjustScreen(*display, prefs_);
            this->setCurrentScreen(*offsetAdjust);
            log_d("setting up callback");
            offsetAdjust->backCallback = [this, offsetAdjust, readingSettingsMenu]() {
                readingSettingsMenu->setFirstLoad();
                setCurrentScreen(*readingSettingsMenu);
                delete offsetAdjust;
            };
            log_d("set up callback");
        }));

    readingSettingsMenu->addMenuItem(
        MenuItem("Resize Page", [this, readingSettingsMenu, display]() {
            log_d("Loading resize box");
            auto resizeBox = new ResizeBoxScreen(*display, prefs_);
            this->setCurrentScreen(*resizeBox);
            log_d("setting up callback");
            resizeBox->backCallback = [this, resizeBox, readingSettingsMenu]() {
                readingSettingsMenu->setFirstLoad();
                setCurrentScreen(*readingSettingsMenu);
                delete resizeBox;
            };
            log_d("set up callback");
        }));

    readingSettingsMenu->addMenuItem(
        MenuItem("Line Spacing", [this, readingSettingsMenu, display]() {
            auto lineSpacingScreen = new LineSpacingScreen(*display, prefs_);
            this->setCurrentScreen(*lineSpacingScreen);
            lineSpacingScreen->backCallback = [this, lineSpacingScreen, readingSettingsMenu]() {
                readingSettingsMenu->setFirstLoad();
                setCurrentScreen(*readingSettingsMenu);
                delete lineSpacingScreen;
            };
        }));

    readingSettingsMenu->addMenuItem(MenuItem("Statistics", [this, readingSettingsMenu, display]() {
        auto statsScreen = new StatsScreen(*display, prefs_);
        this->setCurrentScreen(*statsScreen);
        statsScreen->backCallback = [this, statsScreen, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete statsScreen;
        };
    }));

    systemSettingsMenu->addMenuItem(MenuItem("About", [systemSettingsMenu, display]() {
        std::string snString = StringFormat("SN: %s", GetSerialNumber().c_str());
#ifdef GITHASH
        std::string buildString = StringFormat("Build: %s (%s)", kVersionString, GITHASH);
#else
        std::string buildString = StringFormat("%s (%s)", kVersionString, kGitHash);
#endif

        std::string aboutString = StringFormat("%s, %s", snString.c_str(), buildString.c_str());

        systemSettingsMenu->showTemporaryMessage(*display, aboutString.c_str());
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Tutorials", [systemSettingsMenu, display]() {
        systemSettingsMenu->showTemporaryMessage(*display,
                                                 "Someday we'll implement amazing tutorials!");
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Legal & Regulatory", [systemSettingsMenu, display]() {
        systemSettingsMenu->showTemporaryMessage(*display, "FCC Stuff, Legal Stuff, etc.");
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Set Time Zone", [this, display,
                                                               systemSettingsMenu]() {
        auto timeZoneMenu =
            new MenuScreen(power_.battery, *display, systemSettingsMenu->getMyStyle(),
                           "Time Zone Settings", "System Settings");

        SolClock &clock = SolClock::getInstance();

        timeZoneMenu->setExtraText(clock.timeZoneString());

        auto confirmMessage = [timeZoneMenu, display, &clock](int solTimeZone) {
            clock.setTimezone(solTimeZone);
            timeZoneMenu->showTemporaryMessage(*display, "Setting Clock & Time Zone", 200, false);
            WiFi::getInstance().connectWiFiWithTimeout();
            if (!WiFi::getInstance().connected()) {
                log_e("Couldn't connect to wifi, not setting clock");
            } else {
                log_i("Setting time from internet");
                clock.setClockTimeFromInternet();
                WiFi::getInstance().disconnectWiFi();
            }
            timeZoneMenu->backCallback();
        };

        timeZoneMenu->addMenuItem(
            MenuItem("Pacific Time", [confirmMessage]() { confirmMessage(-8); }));
        timeZoneMenu->addMenuItem(
            MenuItem("Mountain Time", [confirmMessage]() { confirmMessage(-7); }));
        timeZoneMenu->addMenuItem(
            MenuItem("Central Time", [confirmMessage]() { confirmMessage(-6); }));
        timeZoneMenu->addMenuItem(
            MenuItem("Eastern Time", [confirmMessage]() { confirmMessage(-5); }));

        int menuIndex = 0;
        switch (prefs_.solTimeZone.get().value_or(0)) {
        case -8:
            menuIndex = 0;
            break;
        case -7:
            menuIndex = 1;
            break;
        case -6:
            menuIndex = 2;
            break;
        case -5:
            menuIndex = 3;
            break;
        case 0:
        default:
            menuIndex = 0;
            break;
        }

        timeZoneMenu->setCurrSelection(menuIndex);

        setCurrentScreen(*timeZoneMenu);
        timeZoneMenu->backCallback = [this, timeZoneMenu, systemSettingsMenu]() {
            systemSettingsMenu->setFirstLoad();
            setCurrentScreen(*systemSettingsMenu);
            delete timeZoneMenu;
        };
    }));

#if USE_TURN_TOUCH_REMOTE
    debugSettingsMenu->addMenuItem(
        MenuItem("Unpair PageTurner", [this, debugSettingsMenu, display]() {
            prefs_.pairedTurnTouchAddress.remove();
            prefs_.lastPageTurnerBuild.remove();
            PreferencesStore::lastPageTurnerBattery = 0;
            log_w("Unpaired PageTurner. Rebooting...");
            debugSettingsMenu->showTemporaryMessage(*display, "Unpaired PageTurner! Resetting...",
                                                    2000, false);
            power_.reboot();
        }));
#endif

    debugSettingsMenu->addMenuItem(
        MenuItem("Sync Content", [this, display, &localStore, mainMenu, &lightingController]() {
            auto contentSyncScreen = new ContentSyncScreen(*display, localStore, this->prefs_);
            this->setCurrentScreen(*contentSyncScreen);
            lightingController.setOverrideValuePercent(lightingController.getCurrentPercent());
            contentSyncScreen->backCallback = [this, contentSyncScreen, mainMenu, display]() {
                mainMenu->setFirstLoad();
                setCurrentScreen(*mainMenu);
                mainMenu->render(*display);
                delete contentSyncScreen;
            };
            contentSyncScreen->begin();
        }));

    debugSettingsMenu->addMenuItem(
        MenuItem("Turn on Onboarding Mode", [this, debugSettingsMenu, display]() {
            log_d("Entering calibration");
            prefs_.demoMode.set(true);

            debugSettingsMenu->showTemporaryMessage(*display, "Onboarding mode on! Resetting...",
                                                    2000, false);
            power_.reboot();
        }));

    if (hasLastQuestion) {
        debugSettingsMenu->addMenuItem(
            MenuItem("Turn on Demo Mode", [this, debugSettingsMenu, display]() {
                log_d("Demo On!");
                prefs_.cesMode.set(true);
                debugSettingsMenu->showTemporaryMessage(*display, "Demo mode on! Resetting...",
                                                        2000, false);
                power_.reboot();
            }));
    }

    systemSettingsMenu->addMenuItem(
        MenuItem("Reset Statistics", [this, systemSettingsMenu, display]() {
            prefs_.wordCount.set(0);
            prefs_.totalReadingTimeSeconds.set(0);
            systemSettingsMenu->showTemporaryMessage(*display, "Statistics reset.");
        }));

    systemSettingsMenu->addMenuItem(MenuItem(
        "Clear All Content and Restart", [this, systemSettingsMenu, display, &localStore]() {
            localStore.deleteReadables();
            localStore.deleteArticles();
            systemSettingsMenu->showTemporaryMessage(*display, "Content cleared.");
            Delay(1500);
            this->power_.reboot();
        }));

    systemSettingsMenu->addMenuItem(
        MenuItem("Reset Preferences", [this, systemSettingsMenu, display]() {
            prefs_.clear();
            log_w("Preferences cleared. Rebooting...");
            systemSettingsMenu->showTemporaryMessage(*display, "Preferences cleared. Rebooting...",
                                                     2000, false);
            power_.reboot();
        }));

    debugSettingsMenu->addMenuItem(
        MenuItem("Clear Books and Restart", [debugSettingsMenu, display, &localStore]() {
            localStore.deleteReadables();
            debugSettingsMenu->showTemporaryMessage(*display, "Books reset.");
        }));

    debugSettingsMenu->addMenuItem(
        MenuItem("Clear Articles and Restart", [debugSettingsMenu, display, &localStore]() {
            localStore.deleteArticles();
            debugSettingsMenu->showTemporaryMessage(*display, "Articles reset.");
        }));

    debugSettingsMenu->addMenuItem(MenuItem(
        "Clear Crash Reports and Restart", [this, debugSettingsMenu, display, &fileSystem]() {
            FileList coreDumpFiles;
            fileSystem.listDir("/coredumps", coreDumpFiles);
            for (auto &file : coreDumpFiles) {
                log_i("Removing %s", file.c_str());
                fileSystem.remove(StringFormat("%s%s", "/coredumps/", file.c_str()).c_str());
            }
            debugSettingsMenu->showTemporaryMessage(*display, "Crash Reports reset.");
            Delay(1500);
            power_.reboot();
        }));

    systemSettingsMenu->addMenuItem(MenuItem("Factory Defaults", [this, systemSettingsMenu, display,
                                                                  &localStore]() {
#if SIMULATOR
        // Remove the preferences file simprefs.ini
        std::filesystem::remove("simprefs.ini");
        systemSettingsMenu->showTemporaryMessage(*display, "simprefs.ini deleted! Resetting...");
#else
        nvs_flash_erase();
        localStore.deleteArticles(false);
        localStore.deleteReadables(false);
        log_w("NVS and FS cleared. Rebooting...");
        systemSettingsMenu->showTemporaryMessage(*display, "NVS cleared! Resetting...");
#endif
        power_.reboot();
    }));

    debugSettingsMenu->addMenuItem(
        MenuItem("Onboarding Screens", [this, debugSettingsMenu, display, &lightingController]() {
            auto demoScreen = new DemoScreen(*display, prefs_, lightingController);
            this->setCurrentScreen(*demoScreen);
            demoScreen->backCallback = [this, demoScreen, debugSettingsMenu, display]() {
                debugSettingsMenu->setFirstLoad();
                display->clearDisplay();
                setCurrentScreen(*debugSettingsMenu);
                delete demoScreen;
            };
        }));

    debugSettingsMenu->addMenuItem(
        MenuItem("File System Browser", [this, display, debugSettingsMenu, &fileSystem]() {
            auto fsMenu = new MenuScreen(power_.battery, *display, debugSettingsMenu->getMyStyle(),
                                         "FS Browser", "Debug Settings");
            fsMenu->setExtraText("ls of /");

            FileList localFiles;
            fileSystem.listDir("/", localFiles);
            for (const std::string &str : localFiles) {
                log_d("Next File is %s", str.c_str());
                fsMenu->addMenuItem(MenuItem(str.c_str(), [this, fsMenu, &fileSystem, str]() {
                    SFile toSerialOutput = fileSystem.open(StringFormat("/%s", str.c_str()));
                    while (toSerialOutput.available()) {
                        printf("%c", toSerialOutput.read());
                    }
                    this->display_->clearDisplay();
                    fsMenu->setExtraText(StringFormat("output %s to serial", str.c_str()));
                    fsMenu->setNeedsRerender();
                }));
            }
            setCurrentScreen(*fsMenu);
            fsMenu->backCallback = [this, fsMenu, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete fsMenu;
            };
        }));

    debugSettingsMenu->addMenuItem(
        MenuItem("Batteries and Temperature", [this, display, debugSettingsMenu]() {
            auto batteryScreen = new BatteryScreen(*display, prefs_, power_.battery);
            this->setCurrentScreen(*batteryScreen);
            batteryScreen->backCallback = [this, batteryScreen, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete batteryScreen;
            };
        }));

    debugSettingsMenu->addMenuItem(MenuItem("Optics Test", [this, debugSettingsMenu]() {
        auto opticsTestScreen = new CalibrationScreen();
        opticsTestScreen->setOpticsTest();
        this->display_->clearDisplay();
        this->setCurrentScreen(*opticsTestScreen);
        opticsTestScreen->backCallback = [this, opticsTestScreen, debugSettingsMenu]() {
            debugSettingsMenu->setFirstLoad();
            setCurrentScreen(*debugSettingsMenu);
            delete opticsTestScreen;
        };
    }));

    if (hasLastQuestion) {
        debugSettingsMenu->addMenuItem(
            MenuItem("Turn on EMC Mode", [this, debugSettingsMenu, display]() {
                log_d("EMC On!");
                prefs_.emcMode.set(true);
                debugSettingsMenu->showTemporaryMessage(*display, "EMC mode on! Resetting...", 2000,
                                                        false);
                power_.reboot();
            }));

        debugSettingsMenu->addMenuItem(
            MenuItem("Turn off EMC Mode", [this, debugSettingsMenu, display]() {
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
            alignmentScreen->backCallback = [this, alignmentScreen, debugSettingsMenu, display]() {
                debugSettingsMenu->setFirstLoad();
                display->clearDisplay();
                setCurrentScreen(*debugSettingsMenu);
                delete alignmentScreen;
            };
        }));
*/
#if EINKET013TT1
    debugSettingsMenu->addMenuItem(MenuItem("Set Vcom", [this, display, debugSettingsMenu]() {
        auto vcomMenu = new MenuScreen(power_.battery, *display, debugSettingsMenu->getMyStyle(),
                                       "Vcom Settings", "Debug Settings");
        auto *driver = (static_cast<DisplayEinkET013TT1 *>(display))->getDriver();
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
        vcomMenu->backCallback = [this, vcomMenu, debugSettingsMenu]() {
            debugSettingsMenu->setFirstLoad();
            setCurrentScreen(*debugSettingsMenu);
            delete vcomMenu;
        };
    }));
#endif

    debugSettingsMenu->addMenuItem(MenuItem("Set WiFi Power", [this, display, debugSettingsMenu]() {
        auto wifiPowerMenu =
            new MenuScreen(power_.battery, *display, debugSettingsMenu->getMyStyle(),
                           "WiFi Power Settings", "Debug Settings");

        auto powerToString = [](wifi_power_t power) -> std::string {
            switch (power) {
            case WIFI_POWER_19_5dBm:
                return "Max (19.5dBm)";
            case WIFI_POWER_17dBm:
                return "High (17dBm)";
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
            wifiPowerMenu->backCallback();
        };

        wifiPowerMenu->addMenuItem(MenuItem("Max (19.5dBm)", [confirmMessage]() {
            WiFi::getInstance().setWiFiPower(WIFI_POWER_19_5dBm);
            confirmMessage(WIFI_POWER_19_5dBm);
        }));
        wifiPowerMenu->addMenuItem(MenuItem("High (17dBm)", [confirmMessage]() {
            WiFi::getInstance().setWiFiPower(WIFI_POWER_17dBm);
            confirmMessage(WIFI_POWER_17dBm);
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
        case WIFI_POWER_17dBm:
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
        wifiPowerMenu->backCallback = [this, wifiPowerMenu, debugSettingsMenu]() {
            debugSettingsMenu->setFirstLoad();
            setCurrentScreen(*debugSettingsMenu);
            delete wifiPowerMenu;
        };
    }));

#if EINKET013TT1
    debugSettingsMenu->addMenuItem(
        MenuItem("Set Vcom Precise", [this, display, debugSettingsMenu]() {
            auto vcomPreciseScreen = new VcomAdjustScreen(*display, this->prefs_);
            this->display_->clearDisplay();
            setCurrentScreen(*vcomPreciseScreen);
            vcomPreciseScreen->backCallback = [this, vcomPreciseScreen, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete vcomPreciseScreen;
            };
        }));
#endif

    debugSettingsMenu->addMenuItem(MenuItem("Upload Voltage Log", [this, display, &fileSystem,
                                                                   debugSettingsMenu]() {
        auto voltageLogScreen = new VoltageLogUploadScreen(*display, this->prefs_, fileSystem);
        this->setCurrentScreen(*voltageLogScreen);
        (*voltageLogScreen).backCallback = [this, voltageLogScreen, debugSettingsMenu, display]() {
            debugSettingsMenu->setFirstLoad();
            setCurrentScreen(*debugSettingsMenu);
            debugSettingsMenu->render(*display);
            delete voltageLogScreen;
        };
        (*voltageLogScreen).begin();
    }));

    debugSettingsMenu->addMenuItem(
        MenuItem("Format FS", [this, display, &fileSystem, debugSettingsMenu]() {
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

    oldMenu->addMenuItem(MenuItem("Calibration Screen Run Through", [this, oldMenu]() {
        auto calibrationScreen = new CalibrationScreen();
        this->display_->clearDisplay();
        this->setCurrentScreen(*calibrationScreen);
        calibrationScreen->backCallback = [this, calibrationScreen, oldMenu]() {
            setCurrentScreen(*oldMenu);
            delete calibrationScreen;
        };
    }));

    oldMenu->addMenuItem(MenuItem("Deep sleep", [display, oldMenu]() {
        log_w("Deep sleeping. Goodnight.");
        oldMenu->showTemporaryMessage(*display, "Deep sleeping. Goodnight.", 2000, false);
        esp_deep_sleep_start();
    }));

    oldMenu->addMenuItem(MenuItem("Test Bitmap Screen", [this, oldMenu]() {
        auto bitmapScreen = new BitmapScreen(LOGO_BITS, LOGO_WIDTH, LOGO_HEIGHT);
        setCurrentScreen(*bitmapScreen);
        bitmapScreen->backCallback = [this, bitmapScreen, oldMenu]() {
            setCurrentScreen(*oldMenu);
            delete bitmapScreen;
        };
    }));

    mainMenu->addSubMenu(settingsMenu);

    bool inDemo = prefs_.demoMode.get().value_or(false);

    if (inDemo) {
        auto demoScreen = new DemoScreen(*display, prefs_, lightingController);
        demoScreen->backCallback = [this, demoScreen, mainMenu, display]() {
            display->clearDisplay();
            mainMenu->setFirstLoad();
            setCurrentScreen(*mainMenu);
            delete demoScreen;
        };
        this->setCurrentScreen(*demoScreen);
    } else if (prefs_.cesMode.get().value_or(false)) {
        auto cesScreen = new CesScreen(*display, prefs_, lightingController);
        // reset prefs for of The Last Question readable
        log_d("Starting CES mode readable load");
        auto it = std::find_if(readables.begin(), readables.end(), [](const Readable &x) {
            return (x.title.compare("The Last Question") == 0);
        });
        if (it == readables.end()) {
            log_e("Could not find readable: %s", "The Last Question");
        } else {
            log_d("Found readable: %s", (*it).title.c_str());
        }
        auto readableScreen = createReadableScreen(*it);
        cesScreen->backCallback = [this, cesScreen, display, readableScreen]() {
            display->clearDisplay();
            this->display_->setFont(this->display_->style.getFontFamily().readableFont);
            log_i("Setting Readable Screen after focus in CES mode");
            this->setCurrentScreen(*readableScreen);
            delete cesScreen;
        };
        this->setCurrentScreen(*cesScreen);
    } else if (currentReadable.has_value()) {
        // Check if we left-off on a readable and if so, reload that readable by default
        log_d("Attempting to find readable: %s", currentReadable.value().c_str());
        // If we previously had a readable loaded, show that same readable by default

        auto it =
            std::find_if(readables.begin(), readables.end(), [&currentReadable](const Readable &x) {
                return currentReadable == x.uuid;
            });
        if (it != readables.end()) {
            auto readableScreen = createReadableScreen(*it);
            this->display_->setFont(this->display_->style.getFontFamily().readableFont);
            log_i("Setting Readable Screen on boot in %.0f ms", esp_timer_get_time() / 1000.0f);
            this->setCurrentScreen(*readableScreen);
            log_i("Resuming readable %s at place %" PRIu32, (*it).title.c_str(), (*it).place);
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

        log_i("Setting Menu Screen on boot in %.0f ms", esp_timer_get_time() / 1000.0f);
        setCurrentScreen(*mainMenu);

        /*
        auto calibrationScreen = new LogoScreen();
        calibrationScreen->backCallback = [this, calibrationScreen, display, mainMenu]() {
            display->clearScreen();
            Delay(100);

            setCurrentScreen(*mainMenu);
            delete calibrationScreen;
        };
        setCurrentScreen(*calibrationScreen);
        */
    }

    // If we are in demo mode, overright whatever the current screen would have been
    // with the demo code and set the back callback to mainMenu.

    SHOW_HEAP();
}

void RootUI::loop(DisplaySystem &display, RemoteCommand command) {
    // TODO: Move this logic to PowerManager
    if (power_.battery.getDepleted()) {
        // Force render immediately as we are about to shut off
        auto powerDepletedScreen = new PowerDepletedScreen();
        setCurrentScreen(*powerDepletedScreen);
        this->render(display);
        // Wait 5 seconds to be able to see the message
        Delay(5000);
        // Unless wake sources are defined, nothing can wake us up. Goodnight.
        esp_deep_sleep_start();
    }
    Screen::loop(display, command);
}
