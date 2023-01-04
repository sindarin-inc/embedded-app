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

    auto mainMenu = new MainMenuScreen(this->power_, styleFamilies.mainMenu);

    SIZE_OF(*mainMenu);

    auto readables = localStore.readables();
    auto articles = localStore.articles();

    auto createReadableScreen = [this, display, mainMenu, &fileSystem, &lightingController](
                                    const Readable &readable) -> ReadableScreen * {
        // TODO: Would be better to not allocate / delete these all the time
        auto stream = fileSystem.newRendererStream(readable.localFilePath().c_str());
        this->prefs_.lastReadableId.set(readable.uuid);
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
    // If there are no book and no articles, show the Sync Content item
    if (readables.size() == 0 && articles.size() == 0) {
        mainMenu->addMenuItem(
            MenuItem("Sync Content", [this, display, &localStore, mainMenu, &lightingController]() {
                auto contentSyncScreen = new ContentSyncScreen(*display, localStore, this->prefs_);
                lightingController.setOverrideValuePercent(lightingController.getCurrentPercent());
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
    for (Readable &r : readables) {
        mainMenu->addMenuItem(MenuItem(r.title, [this, r, createReadableScreen]() {
            auto readableScreen = createReadableScreen(r);
            this->setCurrentScreen(*readableScreen);
        }));
        if (currentReadable.has_value() && currentReadable.value() == r.uuid) {
            mainMenu->setCurrSelection(mainMenu->getNumItems() - 1);
        }
    }
    SHOW_HEAP();

    // Setup articles menu, soon to be its own "tab"
    if (articles.size()) {
        mainMenu->addMenuItem(MenuItem("Pocket Articles", [this, mainMenu, articles, display,
                                                           createReadableScreen]() {
            log_i("Loading %zu Pocket articles", articles.size());

            auto pocketMenu = new MenuScreen(this->power_, mainMenu->getMyStyle());

            for (const Readable &r : articles) {
                pocketMenu->addMenuItem(MenuItem(r.title, [this, display, pocketMenu, r,
                                                           createReadableScreen]() {
                    display->clearDisplay();
                    auto readableScreen = createReadableScreen(r);
                    setCurrentScreen(*readableScreen);
                    readableScreen->backCallback = [this, display, readableScreen, pocketMenu]() {
                        display->clearDisplay();
                        setCurrentScreen(*pocketMenu);
                        delete readableScreen;
                    };
                }));
            }
            display->clearDisplay();
            setCurrentScreen(*pocketMenu);
            pocketMenu->backCallback = [this, mainMenu, pocketMenu, display]() {
                display->clearDisplay();
                setCurrentScreen(*mainMenu);
                delete pocketMenu;
            };
        }));
    }

    // Settings menus
    auto settingsMenu = new MenuScreen(this->power_, styleFamilies.settingsMenu);
    auto debugMenu = new MenuScreen(this->power_, styleFamilies.settingsMenu);

    // For items we don't want to throw away yet, but this isn't hooked up anywhere
    auto oldMenu = new MenuScreen(this->power_, styleFamilies.settingsMenu);

    debugMenu->backCallback = [this, settingsMenu]() {
        this->display_->clearDisplay();
        settingsMenu->setFirstLoad();
        setCurrentScreen(*settingsMenu);
    };

    // settingsMenu->addMenuItem(MenuItem("Layout Example", [this, settingsMenu, display]() {
    //     auto layoutTestScreen = new LayoutTestScreen(*display);
    //     setCurrentScreen(*layoutTestScreen);
    //     layoutTestScreen->backCallback = [this, layoutTestScreen, settingsMenu]() {
    //         settingsMenu->setFirstLoad();
    //         setCurrentScreen(*settingsMenu);
    //         delete layoutTestScreen;
    //     };
    // }));

    settingsMenu->addMenuItem(
        MenuItem("Sync Content", [this, display, &localStore, settingsMenu, &lightingController]() {
            auto contentSyncScreen = new ContentSyncScreen(*display, localStore, this->prefs_);
            this->setCurrentScreen(*contentSyncScreen);
            lightingController.setOverrideValuePercent(lightingController.getCurrentPercent());
            contentSyncScreen->backCallback = [this, contentSyncScreen, settingsMenu, display]() {
                settingsMenu->setFirstLoad();
                setCurrentScreen(*settingsMenu);
                settingsMenu->render(*display);
                delete contentSyncScreen;
            };
            contentSyncScreen->begin();
        }));

    settingsMenu->addMenuItem(MenuItem("Firmware Update", [this, settingsMenu, display]() {
        auto firmwareUpdateScreen = new FirmwareUpdateScreen(*display, this->prefs_);
        this->setCurrentScreen(*firmwareUpdateScreen);
        firmwareUpdateScreen->backCallback = [this, firmwareUpdateScreen, settingsMenu, display]() {
            settingsMenu->setFirstLoad();
            setCurrentScreen(*settingsMenu);
            settingsMenu->render(*display);
            delete firmwareUpdateScreen;
        };
        firmwareUpdateScreen->begin();
    }));

    settingsMenu->addMenuItem(MenuItem("Reading Font Size", [this, settingsMenu]() {
        auto fontMenu = new MenuScreen(this->power_, settingsMenu->getMyStyle());
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
        fontMenu->backCallback = [this, fontMenu, settingsMenu]() {
            settingsMenu->setFirstLoad();
            setCurrentScreen(*settingsMenu);
            delete fontMenu;
        };
    }));

    settingsMenu->addMenuItem(
        MenuItem("Lighting", [this, display, settingsMenu, &lightingController]() {
            auto lightingAdjustScreen =
                new LightingAdjustScreen(*display, this->prefs_, lightingController);
            this->display_->clearDisplay();
            setCurrentScreen(*lightingAdjustScreen);
            lightingAdjustScreen->backCallback = [this, lightingAdjustScreen, settingsMenu]() {
                settingsMenu->setFirstLoad();
                setCurrentScreen(*settingsMenu);
                delete lightingAdjustScreen;
            };
        }));

    settingsMenu->addMenuItem(MenuItem("Set Line Spacing", [this, settingsMenu, display]() {
        auto lineSpacingScreen = new LineSpacingScreen(*display, prefs_);
        this->setCurrentScreen(*lineSpacingScreen);
        lineSpacingScreen->backCallback = [this, lineSpacingScreen, settingsMenu]() {
            settingsMenu->setFirstLoad();
            setCurrentScreen(*settingsMenu);
            delete lineSpacingScreen;
        };
    }));

    settingsMenu->addMenuItem(MenuItem("Move Text Box", [this, settingsMenu, display]() {
        log_d("Loading adjust offset");
        auto offsetAdjust = new OffsetAdjustScreen(*display, prefs_, this->power_);
        this->setCurrentScreen(*offsetAdjust);
        log_d("setting up callback");
        offsetAdjust->backCallback = [this, offsetAdjust, settingsMenu]() {
            settingsMenu->setFirstLoad();
            setCurrentScreen(*settingsMenu);
            delete offsetAdjust;
        };
        log_d("set up callback");
    }));

    settingsMenu->addMenuItem(MenuItem("Resize Text Box", [this, settingsMenu, display]() {
        log_d("Loading resize box");
        auto resizeBox = new ResizeBoxScreen(*display, prefs_, this->power_);
        this->setCurrentScreen(*resizeBox);
        log_d("setting up callback");
        resizeBox->backCallback = [this, resizeBox, settingsMenu]() {
            settingsMenu->setFirstLoad();
            setCurrentScreen(*settingsMenu);
            delete resizeBox;
        };
        log_d("set up callback");
    }));

    settingsMenu->addMenuItem(MenuItem("Debug", [this, debugMenu]() {
        log_d("Loading Debug Menu");
        debugMenu->setFirstLoad();
        this->setCurrentScreen(*debugMenu);
    }));

    std::string snString = StringFormat("SN: %s", GetSerialNumber().c_str());
    settingsMenu->setExtraText(snString);

#ifdef GITHASH
    std::string buildString = StringFormat("Build: %s (%s)", kVersionString, GITHASH);
#else
    std::string buildString = StringFormat("Build: %s (%s)", kVersionString, kGitHash);
#endif

    debugMenu->addMenuItem(MenuItem("Turn on CES Mode", [this, debugMenu, display]() {
        log_d("CES On!");
        prefs_.cesMode.set(1);
        debugMenu->showTemporaryMessage(*display, "CES mode on! Resetting...", 2000, false);
        esp_restart();
    }));

    debugMenu->addMenuItem(MenuItem("File System Browser", [this, debugMenu, &fileSystem]() {
        MenuStyle styleFamilies;
        auto fsMenu = new MenuScreen(this->power_, debugMenu->getMyStyle());
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
        fsMenu->backCallback = [this, fsMenu, debugMenu]() {
            debugMenu->setFirstLoad();
            setCurrentScreen(*debugMenu);
            delete fsMenu;
        };
    }));

    debugMenu->addMenuItem(MenuItem("Font Type", [this, debugMenu]() {
        MenuStyle styleFamilies;
        auto fontMenu = new MenuScreen(this->power_, debugMenu->getMyStyle());
        fontMenu->setExtraText("Example font type");

        fontMenu->addMenuItem(MenuItem("Lucida 1px space", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.lucidaSkinny);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_LUCIDA_SKINNY);
            fontMenu->needsRerender = true;
        }));
        fontMenu->addMenuItem(MenuItem("Lucida 2px space", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.lucidaFat);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_LUCIDA_FAT);
            fontMenu->needsRerender = true;
        }));
        fontMenu->addMenuItem(MenuItem("Lucida original", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.lucidaRegular);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_LUCIDA_REGULAR);
            fontMenu->needsRerender = true;
        }));

        fontMenu->addMenuItem(MenuItem("Tahoma 5pt", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.tahomaRegular);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_TAHOMA_REGULAR);
            fontMenu->needsRerender = true;
        }));

        fontMenu->addMenuItem(MenuItem("Tahoma 6pt", [this, fontMenu]() {
            this->display_->style.setFontFamily(this->display_->style.tahomaLarge);
            this->display_->clearDisplay();
            prefs_.font.set(PREFS_TAHOMA_LARGE);
            fontMenu->needsRerender = true;
        }));
        setCurrentScreen(*fontMenu);
        fontMenu->backCallback = [this, fontMenu, debugMenu]() {
            debugMenu->setFirstLoad();
            setCurrentScreen(*debugMenu);
            delete fontMenu;
        };
    }));

    debugMenu->addMenuItem(MenuItem("Set Autopilot", [this, debugMenu]() {
        auto speedMenu = new MenuScreen(this->power_, debugMenu->getMyStyle());
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
        speedMenu->backCallback = [this, speedMenu, debugMenu]() {
            debugMenu->setFirstLoad();
            setCurrentScreen(*debugMenu);
            delete speedMenu;
        };
    }));

#if EINKET013TT1
    debugMenu->addMenuItem(MenuItem("Set Vcom", [this, display, debugMenu]() {
        auto vcomMenu = new MenuScreen(this->power_, debugMenu->getMyStyle());
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
        vcomMenu->backCallback = [this, vcomMenu, debugMenu]() {
            debugMenu->setFirstLoad();
            setCurrentScreen(*debugMenu);
            delete vcomMenu;
        };
    }));
#endif

#if EINKET013TT1
    debugMenu->addMenuItem(MenuItem("Set Vcom Precise", [this, display, debugMenu]() {
        auto vcomPreciseScreen = new VcomAdjustScreen(*display, this->prefs_, this->power_);
        this->display_->clearDisplay();
        setCurrentScreen(*vcomPreciseScreen);
        vcomPreciseScreen->backCallback = [this, vcomPreciseScreen, debugMenu]() {
            debugMenu->setFirstLoad();
            setCurrentScreen(*debugMenu);
            delete vcomPreciseScreen;
        };
    }));
#endif

    debugMenu->addMenuItem(MenuItem("Statistics", [this, debugMenu, display]() {
        auto statsScreen = new StatsScreen(*display, prefs_);
        this->setCurrentScreen(*statsScreen);
        statsScreen->backCallback = [this, statsScreen, debugMenu]() {
            debugMenu->setFirstLoad();
            setCurrentScreen(*debugMenu);
            delete statsScreen;
        };
    }));

    debugMenu->addMenuItem(MenuItem("Send Crash Report", [this, debugMenu, display, &fileSystem]() {
        auto crashReportScreen = new CrashReportScreen(*display, this->prefs_, fileSystem);
        this->setCurrentScreen(*crashReportScreen);
        crashReportScreen->backCallback = [this, crashReportScreen, debugMenu]() {
            debugMenu->setFirstLoad();
            setCurrentScreen(*debugMenu);
            delete crashReportScreen;
        };
        crashReportScreen->begin();
    }));

    debugMenu->addMenuItem(MenuItem("Reset Preferences", [this, debugMenu, display]() {
        prefs_.clear();
        log_w("Preferences cleared. Rebooting...");
        debugMenu->showTemporaryMessage(*display, "Preferences cleared. Rebooting...", 2000, false);
        esp_restart();
    }));

    debugMenu->addMenuItem(MenuItem("Reset Statistics", [this, debugMenu, display]() {
        prefs_.wordCount.set(0);
        prefs_.totalReadingTimeSeconds.set(0);
        debugMenu->showTemporaryMessage(*display, "Statistics reset.");
    }));

    debugMenu->addMenuItem(
        MenuItem("Clear Readables and Restart", [debugMenu, display, &localStore]() {
            localStore.clearReadablesFile();
            debugMenu->showTemporaryMessage(*display, "Readables reset.");
        }));

    debugMenu->addMenuItem(
        MenuItem("Clear Articles and Restart", [debugMenu, display, &localStore]() {
            localStore.clearArticlesFile();
            debugMenu->showTemporaryMessage(*display, "Articles reset.");
        }));

#if USE_TURN_TOUCH_REMOTE
    debugMenu->addMenuItem(MenuItem("Unpair PageTurner", [this, debugMenu, display]() {
        prefs_.pairedTurnTouchAddress.remove();
        log_w("Unpaired PageTurner. Rebooting...");
        debugMenu->showTemporaryMessage(*display, "Unpaired PageTurner! Resetting...", 2000, false);
        esp_restart();
    }));
#endif

    debugMenu->addMenuItem(MenuItem("Factory Defaults", [debugMenu, display, &localStore]() {
#if SIMULATOR
        // Remove the preferences file simprefs.ini
        std::filesystem::remove("simprefs.ini");
        debugMenu->showTemporaryMessage(*display, "simprefs.ini deleted! Resetting...");
#else
        nvs_flash_erase();
        localStore.clearArticlesFile(false);
        localStore.clearReadablesFile(false);
        log_w("NVS and FS cleared. Rebooting...");
        debugMenu->showTemporaryMessage(*display, "NVS cleared! Resetting...");
#endif
        esp_restart();
    }));

    debugMenu->addMenuItem(MenuItem("Optics Test", [this, debugMenu]() {
        auto opticsTestScreen = new CalibrationScreen();
        opticsTestScreen->setOpticsTest();
        this->display_->clearDisplay();
        this->setCurrentScreen(*opticsTestScreen);
        opticsTestScreen->backCallback = [this, opticsTestScreen, debugMenu]() {
            debugMenu->setFirstLoad();
            setCurrentScreen(*debugMenu);
            delete opticsTestScreen;
        };
    }));

    debugMenu->addMenuItem(MenuItem("Turn on Onboarding Mode", [this, debugMenu, display]() {
        log_d("Entering calibration");
        prefs_.demoMode.set(1);

        debugMenu->showTemporaryMessage(*display, "Onboarding mode on! Resetting...", 2000, false);
        esp_restart();
    }));

    debugMenu->addMenuItem(
        MenuItem("Onboarding Screens", [this, debugMenu, display, &lightingController]() {
            auto demoScreen = new DemoScreen(*display, prefs_, this->power_, lightingController);
            this->setCurrentScreen(*demoScreen);
            demoScreen->backCallback = [this, demoScreen, debugMenu, display]() {
                debugMenu->setFirstLoad();
                display->clearDisplay();
                setCurrentScreen(*debugMenu);
                delete demoScreen;
            };
        }));

    debugMenu->addMenuItem(
        MenuItem("Alignment Setup", [this, debugMenu, display, &lightingController]() {
            auto alignmentScreen = new AlignmentScreen(*display, prefs_, lightingController);
            this->setCurrentScreen(*alignmentScreen);
            alignmentScreen->backCallback = [this, alignmentScreen, debugMenu, display]() {
                debugMenu->setFirstLoad();
                display->clearDisplay();
                setCurrentScreen(*debugMenu);
                delete alignmentScreen;
            };
        }));

    debugMenu->addMenuItem(MenuItem("Batteries and Temperature", [this, display, debugMenu]() {
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
        debugMenu->setFirstLoad();
    }));

    debugMenu->addMenuItem(
        MenuItem("Upload Voltage Log", [this, display, &fileSystem, debugMenu]() {
            auto voltageLogScreen = new VoltageLogUploadScreen(*display, this->prefs_, fileSystem);
            this->setCurrentScreen(*voltageLogScreen);
            (*voltageLogScreen).backCallback = [this, voltageLogScreen, debugMenu, display]() {
                debugMenu->setFirstLoad();
                setCurrentScreen(*debugMenu);
                debugMenu->render(*display);
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

    debugMenu->setExtraText(buildString);

    settingsMenu->backCallback = [this, mainMenu]() {
        this->display_->clearDisplay();
        mainMenu->setFirstLoad();
        setCurrentScreen(*mainMenu);
    };

    mainMenu->addMenuItem(MenuItem("Settings", [this, settingsMenu]() {
        this->display_->clearDisplay();
        setCurrentScreen(*settingsMenu);
    }));

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
