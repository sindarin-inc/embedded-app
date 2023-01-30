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
#include "GitVersion.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/MenuStyle.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/LocalStore.hpp"
#include "UI/AlignmentScreen.hpp"
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
#include "UI/LoadingScreen.hpp"
#include "UI/LogoScreen.hpp"
#include "UI/MainMenuScreen.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/StatsScreen.hpp"
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

    auto mainMenu = new MainMenuScreen(this->power_, *display_, styleFamilies.mainMenu);

    SIZE_OF(*mainMenu);

    auto readables = localStore.readables();
    auto articles = localStore.articles();

    auto createReadableScreen = [this, display, mainMenu, &fileSystem, &lightingController](
                                    const Readable &readable) -> ReadableScreen * {
        // TODO: Would be better to not allocate / delete these all the time
        auto stream = fileSystem.newRendererStream(readable.localFilePath().c_str());
        auto *readableScreen = new ReadableScreen(stream, *display, readable, this->prefs_,
                                                  this->power_, fileSystem, lightingController);

        // When switching to a readable from a menu, we should clear the display
        // to make the whites really white and the blacks really black.
        display->clearDisplay();
        readableScreen->backCallback = [this, display, stream, readableScreen, mainMenu]() {
            // When leaving a readable to go back to the main menu, do a full clear
            // of the display.
            display->clearDisplay();
            prefs_.lastReadableId.remove();
            mainMenu->setFirstLoad();
            setCurrentScreen(*mainMenu);

            // TODO: Probably not the most kosher practice to delete the `ReadableScreen` from
            // _inside_ ReadableScreen.backCallback. I suppose we could either mark these for
            // deletion on the next loop by adding them to a vector, OR we could rearrange things
            // so that the `backCallback` is copied by the parent screen, and then executed there.
            delete readableScreen;
            delete stream;
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
    if (prefs_.emcMode.get().value() == 1) {
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
    auto it = std::find_if(readables.begin(), readables.end(), [](const Readable &x) {
        return (x.title.compare("Psalms and Proverbs") == 0);
    });
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

    auto booksMenu = new MenuScreen(this->power_, mainMenu->getMyStyle(), "Books");

    // If there are no books, show the Sync Content item in the books tab.
    if (readables.size() == 0) {
        booksMenu->addMenuItem(
            MenuItem("Sync Content", [this, display, &localStore, mainMenu, &lightingController]() {
                auto contentSyncScreen = new ContentSyncScreen(*display, localStore, this->prefs_);
                lightingController.setOverrideValuePercent(lightingController.getCurrentPercent());
                display->setFont(mainMenu->getMyStyle().navigationFont);
                this->setCurrentScreen(*contentSyncScreen);
                contentSyncScreen->backCallback = [this, contentSyncScreen, mainMenu, display]() {
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
        auto title = archived ? "Archived: " + r.title : r.title;
        // Bible books go in their own menu / app
        if ((r.title.compare("Psalms and Proverbs") != 0) &&
            (r.title.compare("New Testament") != 0) && (r.title.compare("Old Testament") != 0)) {
            booksMenu->addMenuItem(MenuItem(
                title, r.authors[0].name, [this, r, createReadableScreen, display, mainMenu]() {
                    auto readableScreen = createReadableScreen(r);
                    this->setCurrentScreen(*readableScreen);
                    readableScreen->backCallback = [this, r, display, readableScreen, mainMenu]() {
                        display->clearDisplay();
                        setCurrentScreen(*mainMenu);
                        delete readableScreen;
                    };
                }));
        }
    }

    booksMenu->backCallback = [this, mainMenu, booksMenu, display]() {
        display->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
        delete booksMenu;
    };

    mainMenu->addSubMenuScreen(booksMenu);

    SHOW_HEAP();

    log_i("Loading %zu Pocket articles", articles.size());

    auto pocketMenu = new MenuScreen(this->power_, mainMenu->getMyStyle(), "Articles");

    // If there are no articles, show the Sync Content item in the articles tab.
    if (articles.size() == 0) {
        pocketMenu->addMenuItem(
            MenuItem("Sync Content", [this, display, &localStore, mainMenu, &lightingController]() {
                auto contentSyncScreen = new ContentSyncScreen(*display, localStore, this->prefs_);
                lightingController.setOverrideValuePercent(lightingController.getCurrentPercent());
                display->setFont(mainMenu->getMyStyle().navigationFont);
                this->setCurrentScreen(*contentSyncScreen);
                contentSyncScreen->backCallback = [this, contentSyncScreen, mainMenu, display]() {
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
        auto title = archived ? "Archived: " + r.title : r.title;

        pocketMenu->addMenuItem(
            MenuItem(title, [this, display, pocketMenu, mainMenu, r, createReadableScreen]() {
                display->clearDisplay();
                auto readableScreen = createReadableScreen(r);
                setCurrentScreen(*readableScreen);
                readableScreen->backCallback = [this, r, display, readableScreen, mainMenu,
                                                pocketMenu]() {
                    display->clearDisplay();
                    bool archived = prefs_.readableArchivedChanged.get(r.shortenedUuid.c_str());
                    auto title = archived ? "Archived: " + r.title : r.title;
                    pocketMenu->changeItemTitleAt(pocketMenu->getCurrSelection(), title);
                    mainMenu->setFirstLoad();
                    setCurrentScreen(*mainMenu);
                    delete readableScreen;
                };
            }));
    }

    pocketMenu->backCallback = [this, mainMenu, pocketMenu, display]() {
        display->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
        delete pocketMenu;
    };

    mainMenu->addSubMenuScreen(pocketMenu);

    if (oldTestament || newTestament || psalmsProverbs) {
        auto bibleMenu = new MenuScreen(this->power_, mainMenu->getMyStyle(), "Bible");

        for (const Readable &r : readables) {
            bool archived = prefs_.readableArchivedChanged.get(r.shortenedUuid.c_str());
            auto title = archived ? "Archived: " + r.title : r.title;
            if ((r.title.compare("Psalms and Proverbs") == 0) ||
                (r.title.compare("New Testament") == 0) ||
                (r.title.compare("Old Testament") == 0)) {
                bibleMenu->addMenuItem(MenuItem(title, [this, r, createReadableScreen, display,
                                                        mainMenu]() {
                    auto readableScreen = createReadableScreen(r);
                    this->setCurrentScreen(*readableScreen);
                    readableScreen->backCallback = [this, r, display, readableScreen, mainMenu]() {
                        display->clearDisplay();
                        setCurrentScreen(*mainMenu);
                        delete readableScreen;
                    };
                }));
            }
        }

        bibleMenu->backCallback = [this, mainMenu, bibleMenu, display]() {
            display->clearDisplay();
            mainMenu->setFirstLoad();
            setCurrentScreen(*mainMenu);
            delete bibleMenu;
        };
        mainMenu->addSubMenuScreen(bibleMenu);
    }

    // Settings menus
    // Sync Content
    // Reading Settings
    // System Settings
    // Debug Settings
    auto settingsMenu = new MenuScreen(this->power_, styleFamilies.settingsMenu, "Settings");

    // Font Size
    // Line Spacing
    // Brightness
    // Move Text Box
    // Resize Text Box
    // Set Autopilot
    auto readingSettingsMenu = new MenuScreen(this->power_, styleFamilies.settingsMenu);

    readingSettingsMenu->backCallback = [this, mainMenu]() {
        this->display_->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
    };

    // Firmware Update
    // Send Crash Report
    // Batteries and Temperature
    // Turn on Onboarding Mode
    // Turn on CES Mode
    // Statistics
    // Reset Statistics
    // Reset Preferences
    // Clear Books and Restart
    // Clear Articles and Restart
    // Factory Defaults
    auto systemSettingsMenu = new MenuScreen(this->power_, styleFamilies.settingsMenu);

    systemSettingsMenu->backCallback = [this, mainMenu]() {
        this->display_->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
    };

    // Unpair PageTurner
    // Onboarding Screens
    // File System Browser
    // Optics Test
    // Turn on EMC Mode
    // Turn off EMC Mode
    // Alignment Setup
    // Set Vcom
    // Set Vcom Precise
    // Upload Voltage Log
    auto debugSettingsMenu = new MenuScreen(this->power_, styleFamilies.settingsMenu);

    debugSettingsMenu->backCallback = [this, mainMenu]() {
        this->display_->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
    };

    // For items we don't want to throw away yet, but this isn't hooked up anywhere
    auto oldMenu = new MenuScreen(this->power_, styleFamilies.settingsMenu);

    // settingsMenu->addMenuItem(MenuItem("Layout Example", [this, settingsMenu, display]() {
    //    auto layoutTestScreen = new LayoutTestScreen(*display);
    //    setCurrentScreen(*layoutTestScreen);
    //    layoutTestScreen->backCallback = [this, layoutTestScreen, settingsMenu]() {
    //       settingsMenu->setFirstLoad();
    //        setCurrentScreen(*settingsMenu);
    //        delete layoutTestScreen;
    //    };
    // }));

    settingsMenu->addMenuItem(
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

    settingsMenu->addMenuItem(MenuItem("Reading", [this, readingSettingsMenu]() {
        log_d("Loading Reading Settings Menu");
        readingSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*readingSettingsMenu);
    }));

    settingsMenu->addMenuItem(MenuItem("System", [this, systemSettingsMenu]() {
        log_d("Loading System Settings Menu");
        systemSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*systemSettingsMenu);
    }));

    settingsMenu->addMenuItem(MenuItem("Debug", [this, debugSettingsMenu]() {
        log_d("Loading Debug Settings Menu");
        debugSettingsMenu->setFirstLoad();
        this->setCurrentScreen(*debugSettingsMenu);
    }));

    readingSettingsMenu->addMenuItem(MenuItem("Font Size", [this, readingSettingsMenu]() {
        auto fontMenu = new MenuScreen(this->power_, readingSettingsMenu->getMyStyle());
        fontMenu->setExtraText("Example font size");
        fontMenu->addMenuItem(MenuItem("Small", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.tahomaRegular);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_TAHOMA_REGULAR);
            fontMenu->needsRerender = true;
        }));
        fontMenu->addMenuItem(MenuItem("Large", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.tahomaLarge);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_TAHOMA_LARGE);
            fontMenu->needsRerender = true;
        }));
        setCurrentScreen(*fontMenu);
        fontMenu->backCallback = [this, fontMenu, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete fontMenu;
        };
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
        MenuItem("Move Text Box", [this, readingSettingsMenu, display]() {
            log_d("Loading adjust offset");
            auto offsetAdjust = new OffsetAdjustScreen(*display, prefs_, this->power_);
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
        MenuItem("Resize Text Box", [this, readingSettingsMenu, display]() {
            log_d("Loading resize box");
            auto resizeBox = new ResizeBoxScreen(*display, prefs_, this->power_);
            this->setCurrentScreen(*resizeBox);
            log_d("setting up callback");
            resizeBox->backCallback = [this, resizeBox, readingSettingsMenu]() {
                readingSettingsMenu->setFirstLoad();
                setCurrentScreen(*readingSettingsMenu);
                delete resizeBox;
            };
            log_d("set up callback");
        }));

    readingSettingsMenu->addMenuItem(MenuItem("Set Autopilot", [this, readingSettingsMenu]() {
        auto speedMenu = new MenuScreen(this->power_, readingSettingsMenu->getMyStyle());
        speedMenu->addMenuItem(MenuItem("None (Off)", [this, speedMenu]() {
            prefs_.autoPilotSpeed.set(0);
            speedMenu->setExtraText("Autopilot off");
            speedMenu->needsRerender = true;
        }));
        speedMenu->addMenuItem(MenuItem("Fast (On)", [this, speedMenu]() {
            prefs_.autoPilotSpeed.set(5000);
            speedMenu->setExtraText("Autopilot on");
            speedMenu->needsRerender = true;
        }));
        speedMenu->addMenuItem(MenuItem("Slow (On)", [this, speedMenu]() {
            prefs_.autoPilotSpeed.set(10000);
            speedMenu->setExtraText("Autopilot on");
            speedMenu->needsRerender = true;
        }));
        if (prefs_.autoPilotSpeed.get() == 0) {
            speedMenu->setExtraText("Autopilot off");
        } else {
            speedMenu->setExtraText("Autopilot on");
        }
        setCurrentScreen(*speedMenu);
        speedMenu->backCallback = [this, speedMenu, readingSettingsMenu]() {
            readingSettingsMenu->setFirstLoad();
            setCurrentScreen(*readingSettingsMenu);
            delete speedMenu;
        };
    }));

    std::string snString = StringFormat("SN: %s", GetSerialNumber().c_str());
    systemSettingsMenu->setExtraText(snString);

#ifdef GITHASH
    std::string buildString = StringFormat("Build: %s (%s)", kVersionString, GITHASH);
#else
    std::string buildString = StringFormat("Build: %s (%s)", kVersionString, kGitHash);
#endif

    systemSettingsMenu->addMenuItem(
        MenuItem("Firmware Update", [this, systemSettingsMenu, display]() {
            auto firmwareUpdateScreen = new FirmwareUpdateScreen(*display, this->prefs_);
            this->setCurrentScreen(*firmwareUpdateScreen);
            firmwareUpdateScreen->backCallback = [this, firmwareUpdateScreen, systemSettingsMenu,
                                                  display]() {
                systemSettingsMenu->setFirstLoad();
                setCurrentScreen(*systemSettingsMenu);
                systemSettingsMenu->render(*display);
                delete firmwareUpdateScreen;
            };
            firmwareUpdateScreen->begin();
        }));

    FileList coreDumpFiles;
    fileSystem.listDir("/coredumps", coreDumpFiles);

    std::string crashMenuName = StringFormat("Send Crash Report (%d)", coreDumpFiles.size());

    systemSettingsMenu->addMenuItem(MenuItem(crashMenuName, [this, systemSettingsMenu, display,
                                                             &fileSystem]() {
        auto crashReportScreen = new CrashReportScreen(*display, this->prefs_, fileSystem);
        this->setCurrentScreen(*crashReportScreen);
        crashReportScreen->backCallback = [this, display, crashReportScreen, systemSettingsMenu]() {
            systemSettingsMenu->setFirstLoad();
            setCurrentScreen(*systemSettingsMenu);
            systemSettingsMenu->render(*display);
            delete crashReportScreen;
        };
        crashReportScreen->begin();
    }));

    systemSettingsMenu->addMenuItem(MenuItem("Batteries and Temperature", [this, display,
                                                                           systemSettingsMenu]() {
        uint16_t width = 0, height = 0;
        display->setFont(display->style.getFontFamily().readableFont);
        auto batteryStatus = this->power_.getBatteryLifeString();
        uint16_t leftTemp;
#if EINKET013TT1
        auto *driver = ((DisplayEinkET013TT1 *)display)->getDriver();
        // TODO: Add UI for right screen temp also
        leftTemp = driver->readTemperature(ScreenSide::Left);
#else
        leftTemp = 99;
#endif
        batteryStatus += StringFormat(" %dC", leftTemp);

        width = display->getTextWidth(batteryStatus.c_str());
        height = display->getTextHeight(batteryStatus.c_str());

        display->drawSingleLineOfText(batteryStatus.c_str(),
                                      display->xAbsolute(display->xAbsoluteCenter() - width / 2),
                                      display->yAbsolute(display->yAbsoluteCenter() - height / 2));

        auto ptBatteryPct = this->prefs_.lastPageTurnerBattery.get().value_or(0);
        auto ptBuild = this->prefs_.lastPageTurnerBuild.get().value_or("Unknown build");
        auto ptBatteryStatus =
            StringFormat("Page Turner: %d%% (%s)", ptBatteryPct, ptBuild.c_str());
        width = display->getTextWidth(ptBatteryStatus.c_str());
        height = display->getTextHeight(ptBatteryStatus.c_str());
        display->drawSingleLineOfText(ptBatteryStatus.c_str(),
                                      display->xAbsolute(display->xAbsoluteCenter() - width / 2),
                                      display->yAbsolute(display->yAbsoluteCenter() + height / 2));

        display->updateDisplay();
        systemSettingsMenu->setFirstLoad();
    }));

    systemSettingsMenu->addMenuItem(
        MenuItem("Turn on Onboarding Mode", [this, systemSettingsMenu, display]() {
            log_d("Entering calibration");
            prefs_.demoMode.set(1);

            systemSettingsMenu->showTemporaryMessage(*display, "Onboarding mode on! Resetting...",
                                                     2000, false);
            prefs_.lastBatteryReport.set((int)this->power_.getBatteryPercentage());
            esp_restart();
        }));

    systemSettingsMenu->addMenuItem(
        MenuItem("Turn on CES Mode", [this, systemSettingsMenu, display]() {
            log_d("CES On!");
            prefs_.cesMode.set(1);
            systemSettingsMenu->showTemporaryMessage(*display, "CES mode on! Resetting...", 2000,
                                                     false);
            prefs_.lastBatteryReport.set((int)this->power_.getBatteryPercentage());
            esp_restart();
        }));

    systemSettingsMenu->addMenuItem(MenuItem("Statistics", [this, systemSettingsMenu, display]() {
        auto statsScreen = new StatsScreen(*display, prefs_);
        this->setCurrentScreen(*statsScreen);
        statsScreen->backCallback = [this, statsScreen, systemSettingsMenu]() {
            systemSettingsMenu->setFirstLoad();
            setCurrentScreen(*systemSettingsMenu);
            delete statsScreen;
        };
    }));

    systemSettingsMenu->addMenuItem(
        MenuItem("Reset Statistics", [this, systemSettingsMenu, display]() {
            prefs_.wordCount.set(0);
            prefs_.totalReadingTimeSeconds.set(0);
            systemSettingsMenu->showTemporaryMessage(*display, "Statistics reset.");
        }));

    systemSettingsMenu->addMenuItem(
        MenuItem("Reset Preferences", [this, systemSettingsMenu, display]() {
            prefs_.clear();
            log_w("Preferences cleared. Rebooting...");
            systemSettingsMenu->showTemporaryMessage(*display, "Preferences cleared. Rebooting...",
                                                     2000, false);
            prefs_.lastBatteryReport.set((int)this->power_.getBatteryPercentage());
            esp_restart();
        }));

    systemSettingsMenu->addMenuItem(
        MenuItem("Clear Books and Restart", [systemSettingsMenu, display, &localStore]() {
            localStore.deleteReadables();
            systemSettingsMenu->showTemporaryMessage(*display, "Books reset.");
        }));

    systemSettingsMenu->addMenuItem(
        MenuItem("Clear Articles and Restart", [systemSettingsMenu, display, &localStore]() {
            localStore.deleteArticles();
            systemSettingsMenu->showTemporaryMessage(*display, "Articles reset.");
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
        prefs_.lastBatteryReport.set((int)this->power_.getBatteryPercentage());
        esp_restart();
    }));

#if USE_TURN_TOUCH_REMOTE
    debugSettingsMenu->addMenuItem(
        MenuItem("Unpair PageTurner", [this, debugSettingsMenu, display]() {
            prefs_.pairedTurnTouchAddress.remove();
            log_w("Unpaired PageTurner. Rebooting...");
            debugSettingsMenu->showTemporaryMessage(*display, "Unpaired PageTurner! Resetting...",
                                                    2000, false);
            prefs_.lastBatteryReport.set((int)this->power_.getBatteryPercentage());
            esp_restart();
        }));
#endif

    debugSettingsMenu->addMenuItem(
        MenuItem("Onboarding Screens", [this, debugSettingsMenu, display, &lightingController]() {
            auto demoScreen = new DemoScreen(*display, prefs_, this->power_, lightingController);
            this->setCurrentScreen(*demoScreen);
            demoScreen->backCallback = [this, demoScreen, debugSettingsMenu, display]() {
                debugSettingsMenu->setFirstLoad();
                display->clearDisplay();
                setCurrentScreen(*debugSettingsMenu);
                delete demoScreen;
            };
        }));

    debugSettingsMenu->addMenuItem(
        MenuItem("File System Browser", [this, debugSettingsMenu, &fileSystem]() {
            MenuStyle styleFamilies;
            auto fsMenu = new MenuScreen(this->power_, debugSettingsMenu->getMyStyle());
            fsMenu->setExtraText("ls of /");

            FileList localFiles;
            fileSystem.listDir("/", localFiles);
            for (const std::string &str : localFiles) {
                log_d("Next File is %s", str.c_str());
                fsMenu->addMenuItem(MenuItem(str.c_str(), [this, fsMenu, &fileSystem, str]() {
                    File toSerialOutput = fileSystem.open(StringFormat("/%s", str.c_str()));
                    while (toSerialOutput.available()) {
                        printf("%c", toSerialOutput.read());
                    }
                    this->display_->clearDisplay();
                    fsMenu->setExtraText(StringFormat("output %s to serial", str.c_str()));
                    fsMenu->needsRerender = true;
                }));
            }
            setCurrentScreen(*fsMenu);
            fsMenu->backCallback = [this, fsMenu, debugSettingsMenu]() {
                debugSettingsMenu->setFirstLoad();
                setCurrentScreen(*debugSettingsMenu);
                delete fsMenu;
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

    debugSettingsMenu->addMenuItem(MenuItem("Turn on EMC Mode", [this, debugSettingsMenu,
                                                                 display]() {
        log_d("EMC On!");
        prefs_.emcMode.set(1);
        debugSettingsMenu->showTemporaryMessage(*display, "EMC mode on! Resetting...", 2000, false);
        prefs_.lastBatteryReport.set((int)this->power_.getBatteryPercentage());
        esp_restart();
    }));

    debugSettingsMenu->addMenuItem(
        MenuItem("Turn off EMC Mode", [this, debugSettingsMenu, display]() {
            log_d("EMC Off!");
            prefs_.emcMode.set(0);
            debugSettingsMenu->showTemporaryMessage(*display, "EMC mode off! Resetting...", 2000,
                                                    false);
            prefs_.lastBatteryReport.set((int)this->power_.getBatteryPercentage());
            esp_restart();
        }));

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

#if EINKET013TT1
    debugSettingsMenu->addMenuItem(MenuItem("Set Vcom", [this, display, debugSettingsMenu]() {
        auto vcomMenu = new MenuScreen(this->power_, debugSettingsMenu->getMyStyle());
        auto *driver = (static_cast<DisplayEinkET013TT1 *>(display))->getDriver();
        vcomMenu->addMenuItem(MenuItem("0V", [this, vcomMenu, driver]() {
            driver->setVcom(0);
            prefs_.vcom.set(0);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-1V", [this, vcomMenu, driver]() {
            driver->setVcom(-1000);
            prefs_.vcom.set(-1000);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-2V", [this, vcomMenu, driver]() {
            driver->setVcom(-2000);
            prefs_.vcom.set(-2000);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-3V", [this, vcomMenu, driver]() {
            driver->setVcom(-3000);
            prefs_.vcom.set(-3000);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-4V", [this, vcomMenu, driver]() {
            driver->setVcom(-4000);
            prefs_.vcom.set(-4000);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-5V", [this, vcomMenu, driver]() {
            driver->setVcom(-5000);
            prefs_.vcom.set(-5000);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-5.6V", [this, vcomMenu, driver]() {
            driver->setVcom(-5600);
            prefs_.vcom.set(-5600);
            vcomMenu->needsRerender = true;
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
        auto wifiPowerMenu = new MenuScreen(this->power_, debugSettingsMenu->getMyStyle());

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

        wifiPowerMenu->setExtraText(powerToString(GetWiFiPower()));

        auto confirmMessage = [this, wifiPowerMenu, display](wifi_power_t power) {
            this->prefs_.wifiPower.set(power);
            wifiPowerMenu->showTemporaryMessage(*display, "Power set", 200, false);
            wifiPowerMenu->backCallback();
        };

        wifiPowerMenu->addMenuItem(MenuItem("Max (19.5dBm)", [confirmMessage]() {
            SetWiFiPower(WIFI_POWER_19_5dBm);
            confirmMessage(WIFI_POWER_19_5dBm);
        }));
        wifiPowerMenu->addMenuItem(MenuItem("High (17dBm)", [confirmMessage]() {
            SetWiFiPower(WIFI_POWER_17dBm);
            confirmMessage(WIFI_POWER_17dBm);
        }));
        wifiPowerMenu->addMenuItem(MenuItem("Medium (11dBm)", [confirmMessage]() {
            SetWiFiPower(WIFI_POWER_11dBm);
            confirmMessage(WIFI_POWER_11dBm);
        }));
        wifiPowerMenu->addMenuItem(MenuItem("Low (5dBm)", [confirmMessage]() {
            SetWiFiPower(WIFI_POWER_5dBm);
            confirmMessage(WIFI_POWER_5dBm);
        }));
        wifiPowerMenu->addMenuItem(MenuItem("Min (-1dBm)", [confirmMessage]() {
            SetWiFiPower(WIFI_POWER_MINUS_1dBm);
            confirmMessage(WIFI_POWER_MINUS_1dBm);
        }));

        int menuIndex = 0;
        switch (GetWiFiPower()) {
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
            auto vcomPreciseScreen = new VcomAdjustScreen(*display, this->prefs_, this->power_);
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

    oldMenu->addMenuItem(MenuItem("Loading Test", [this, oldMenu]() {
        auto loadingScreen = new LoadingScreen();
        setCurrentScreen(*loadingScreen);
        loadingScreen->backCallback = [this, loadingScreen, oldMenu]() {
            setCurrentScreen(*oldMenu);
            delete loadingScreen;
        };
    }));

    oldMenu->addMenuItem(MenuItem("Test Bitmap Screen", [this, oldMenu]() {
        auto bitmapScreen = new BitmapScreen(LOGO_BITS, LOGO_WIDTH, LOGO_HEIGHT);
        setCurrentScreen(*bitmapScreen);
        bitmapScreen->backCallback = [this, bitmapScreen, oldMenu]() {
            setCurrentScreen(*oldMenu);
            delete bitmapScreen;
        };
    }));

    debugSettingsMenu->setExtraText(buildString);

    // Shouldn't matter anymore.
    settingsMenu->backCallback = [this, mainMenu]() {
        this->display_->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
    };

    mainMenu->addSubMenuScreen(settingsMenu);

    int inDemo = prefs_.demoMode.get().value();

    if (inDemo) {
        auto demoScreen = new DemoScreen(*display, prefs_, this->power_, lightingController);
        demoScreen->backCallback = [this, demoScreen, mainMenu, display]() {
            display->clearDisplay();
            setCurrentScreen(*mainMenu);
            delete demoScreen;
        };
        this->setCurrentScreen(*demoScreen);
    } else if (prefs_.cesMode.get().value()) {
        auto cesScreen = new CesScreen(*display, prefs_, this->power_, lightingController);
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
            log_i("Resuming readable %s at place %d", (*it).title.c_str(), (*it).place);
        } else {
            // Assume this readable has gone away (or changed from id to uuid)
            prefs_.lastReadableId.remove();
        }
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
            delay(100);

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
    if (this->power_.getDepleted()) {
        // Force render immediately as we are about to shut off
        auto powerDepletedScreen = new PowerDepletedScreen();
        setCurrentScreen(*powerDepletedScreen);
        this->render(display);
        // Wait 5 seconds to be able to see the message
        delay(5000);
        // Unless wake sources are defined, nothing can wake us up. Goodnight.
        esp_deep_sleep_start();
    }
    Screen::loop(display, command);
}
