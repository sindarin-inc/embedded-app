#include "RootUI.hpp"

#include <nvs_flash.h>

#include "Bitmaps/Logo.hpp"
#include "Communications/HtmlCleaner.hpp"
#include "Communications/WiFi.hpp"
#include "Contents/Readable.hpp"
#include "GitVersion.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/MenuStyle.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/CalibrationScreen.hpp"
#include "UI/DemoScreen.hpp"
#include "UI/FirmwareUpdateScreen.hpp"
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
#include "config.h"
#include "sindarin-debug.h"

#if ENABLE_POCKET
#include "Communications/PocketAPI.hpp"
#endif

#if USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
#endif

#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#include "UI/VcomAdjustScreen.hpp"
#endif

#define LAST_READABLE_ID "rui-readable"

void RootUI::setup(DisplaySystem *display, std::vector<Readable> &readables,
                   FileSystem &fileSystem) {
    SHOW_HEAP();

    display_ = display;

    MenuStyle styleFamilies;

    auto mainMenu = new MainMenuScreen(this->power_, styleFamilies.mainMenu);

    SIZE_OF(*mainMenu);

    auto createReadableScreen = [this, display, mainMenu,
                                 &fileSystem](Readable *readable) -> ReadableScreen * {
#if !DISABLE_WIFI
        DisconnectWifi();
#endif
        // TODO: Would be better to not allocate / delete these all the time
        auto stream = fileSystem.newRendererStream(readable->path.c_str());
        this->prefs_.lastReadableId.set(readable->id);
        auto *readableScreen =
            new ReadableScreen(stream, *display, *readable, this->prefs_, this->power_);
        if (readable->id.rfind("pkt-", 0) == 0) {
            readableScreen->setLeftPopupButton("Delete", [readable, &fileSystem]() {
                log_i("Deleting Pocket readable %s", readable->path.c_str());
                if (!fileSystem.remove(readable->path)) {
                    log_e("Failed to delete Pocket readable %s", readable->path.c_str());
                }
                log_d("Rebooting to reload readables");
                // TODO: Dynamically reload readables instead of rebooting
                esp_restart();
            });
        }
        // When switching to a readable from a menu, we should clear the display
        // to make the whites really white and the blacks really black.
        display->clearDisplay();
        readableScreen->backCallback = [this, display, stream, readableScreen, mainMenu]() {
#if !DISABLE_WIFI
            ConnectWiFiIfCredentialsExist();
#endif
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
    for (Readable &r : readables) {
        Readable *readable = &r;
        r.place = prefs_.readablePlace.get(r.id.c_str());
        mainMenu->addMenuItem(MenuItem(r.name, [this, readable, createReadableScreen]() {
            auto readableScreen = createReadableScreen(readable);
            this->setCurrentScreen(*readableScreen);
        }));
        if (currentReadable.has_value() && currentReadable.value() == r.id) {
            mainMenu->setCurrSelection(mainMenu->getNumItems() - 1);
        }
    }
    SHOW_HEAP();

    // Set up pocket article menu
#if ENABLE_POCKET
    mainMenu->addMenuItem(MenuItem("Pocket Articles", [this, mainMenu, display, &fileSystem,
                                                       createReadableScreen]() {
        if (!NetworkConnected()) {
            log_w("Tried to load Pocket articles but WiFi not connected");
            display->drawSingleLineOfText("WiFi Disconnected", 0, 0);
            display->updateDisplay();
            delay(2000);
            needsRerender = true;
            return;
        }

        if (!prefs_.pocketApiToken.get().has_value()) {
            log_w("No Pocket API token found");
            display->drawSingleLineOfText("No Pocket API token found", 0, 0);
            display->updateDisplay();
            delay(2000);
            needsRerender = true;
            return;
        }

        log_i("Loading Pocket articles");
        // TODO(John): I wanted to make a loading indicator for this but then realized I'll
        // need to add threading / tasks since the HTTP code is synchronous right now.
        display->drawSingleLineOfText("Downloading...", 0, 0);
        display->updateDisplay();
        PocketAPI *pocket = PocketAPI::getInstance();
        pocket->setAccessToken(prefs_.pocketApiToken.get().value());

        // Since we're reading this directly out of this from the closures, this needs to exist
        // until the closures no longer exist. That or we need to copy some strings.
        SpiRamJsonDocument *response;

        int code = pocket->get(&response);
        if (code != 200) {
            log_e("Pocket HTTP error: %d", code);
            display->drawSingleLineOfText(StringFormat("HTTP error: %d", code), 0, 0);
            display->updateDisplay();
            delay(2000);
            setCurrentScreen(*mainMenu);
            return;
        }

        JsonObject list = (*response)["list"].as<JsonObject>();
        if (list.size() <= 0) {
            log_w("No Pocket articles found");
            display->drawSingleLineOfText("No articles found", 0, 0);
            display->updateDisplay();
            delay(2000);
            setCurrentScreen(*mainMenu);
            delete response;
            return;
        }

        log_i("Pocket /get request succeeded");

        auto pocketMenu = new MenuScreen(this->power_, mainMenu->getMyStyle());

        for (JsonPair keyValue : list) {
            JsonObject article = keyValue.value().as<JsonObject>();
            const char *url = article["resolved_url"].as<const char *>();
            log_i("Got Pocket url %s", url);
            const char *title = article["resolved_title"].as<const char *>();
            pocketMenu->addMenuItem(MenuItem(title, [this, pocketMenu, title, url, pocket, display,
                                                     &fileSystem, createReadableScreen]() {
                log_d("Loading article %s (%s)", title, url);
                display->drawSingleLineOfText("Downloading...", 0, 0);
                display->updateDisplay();
                SpiRamJsonDocument *response;
                auto code = pocket->text(url, &response);
                if (code != 200) {
                    if (code == -1) {
                        log_e("Pocket HTTP Error: %d (Article too long?)", code);
                        display->drawSingleLineOfText("Error: Too long", 0, 0);
                        display->updateDisplay();
                    } else {
                        log_e("Pocket HTTP Error: %d", code);
                        display->drawSingleLineOfText(StringFormat("Pocket Error: %d", code), 0, 0);
                        display->updateDisplay();
                    }
                    delay(2000);
                    setCurrentScreen(*pocketMenu);
                    return;
                }
                const char *text = (*response)["article"].as<const char *>();
                log_i("Pocket text succeeded for %s", url);
                std::string str = text;

                if (!fileSystem.createDir("/pocket")) {
                    log_e("Unable to create Pocket dir");
                    display->drawSingleLineOfText(StringFormat("FS error createDir"), 0, 0);
                    display->updateDisplay();
                    delay(2000);
                    setCurrentScreen(*pocketMenu);
                    return;
                }
                auto id = StringFormat("pkt-%s", (*response)["resolved_id"].as<const char *>());
                auto path = StringFormat("/pocket/%s.txt", id.c_str());
                File file = fileSystem.open(path.c_str(), FILE_WRITE);
                const char *title = (*response)["title"].as<const char *>();
                file.write((const uint8_t *)title, strlen(title));
                file.write('\n');

                StripHtml(str, file);

                file.flush();
                file.close();

                auto readable = new Readable(id.c_str(), title, path.c_str());

                auto readableScreen = createReadableScreen(readable);
                setCurrentScreen(*readableScreen);

                readableScreen->backCallback = [this, readableScreen, pocketMenu, response,
                                                readable]() {
                    setCurrentScreen(*pocketMenu);
                    delete readableScreen;
                    delete response;
                    delete readable;
                };
            }));
        }
        display->clearDisplay();
        setCurrentScreen(*pocketMenu);
        pocketMenu->backCallback = [this, mainMenu, pocketMenu, response, display]() {
            display->clearDisplay();
            setCurrentScreen(*mainMenu);
            delete pocketMenu;
            delete response;
        };
    }));
#endif // DISABLE_WIFI

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

    settingsMenu->addMenuItem(MenuItem("Lighting", [this, settingsMenu]() {
        auto lightingAdjustScreen = new LightingAdjustScreen(this->prefs_);
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

#ifdef GITHASH
    std::string buildString = "Build " + std::string(GITHASH);
#else
    std::string buildString = "Build " + std::string(kGitHash);
#endif

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

    debugMenu->addMenuItem(MenuItem("Reset Preferences", [this, display]() {
        prefs_.clear();
        log_w("Preferences cleared. Rebooting...");

        display->drawSingleLineOfText(
            "Preferences cleared! Resetting...",
            display->xAbsolute(display->xAbsoluteCenter() -
                               display->getTextWidth("Preferences cleared! Resetting...") / 2),
            display->yAbsolute(display->yAbsoluteCenter() - display->getTextHeight() / 2));
        display->updateDisplay();
        delay(2000);
        esp_restart();
    }));

    debugMenu->addMenuItem(MenuItem("Reset Statistics", [this, display]() {
        prefs_.wordCount.set(0);
        prefs_.totalReadingTimeSeconds.set(0);
        display->drawSingleLineOfText(
            "Statistics reset.",
            display->xAbsolute(display->xAbsoluteCenter() -
                               display->getTextWidth("Statistics reset.") / 2),
            display->yAbsolute(display->yAbsoluteCenter() - display->getTextHeight() / 2));
        display->updateDisplay();
    }));

#if USE_TURN_TOUCH_REMOTE
    debugMenu->addMenuItem(MenuItem("Unpair TurnTouch", [this, display]() {
        prefs_.pairedTurnTouchAddress.remove();
        log_w("Unpaired TurnTouch. Rebooting...");
        display->drawSingleLineOfText(
            "Unpaired TurnTouch! Resetting...",
            display->xAbsolute(display->xAbsoluteCenter() -
                               display->getTextWidth("Unpaired TurnTouch! Resetting...") / 2),
            display->yAbsolute(display->yAbsoluteCenter() - display->getTextHeight() / 2));
        display->updateDisplay();
        delay(2000);
        esp_restart();
    }));
#endif

    debugMenu->addMenuItem(
        MenuItem("Factory Defaults", [this]() { this->restoreFactoryDefaults(); }));

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

    debugMenu->addMenuItem(MenuItem("Turn on Onboarding Mode", [this, display]() {
        log_d("Entering calibration");
        prefs_.demoMode.set(1);
        display->drawSingleLineOfText(
            "Onboarding mode on! Resetting...",
            display->xAbsolute(display->xAbsoluteCenter() -
                               display->getTextWidth("Onboarding mode on! Resetting...") / 2),
            display->yAbsolute(
                display->yAbsolute(display->yAbsoluteCenter() - display->getTextHeight() / 2)));
        display->updateDisplay();
        delay(2000);
        esp_restart();
    }));

    debugMenu->addMenuItem(MenuItem("Onboarding Screens", [this, debugMenu, display]() {
        auto demoScreen = new DemoScreen(*display, prefs_, this->power_);
        this->setCurrentScreen(*demoScreen);
        demoScreen->backCallback = [this, demoScreen, debugMenu]() {
            debugMenu->setFirstLoad();
            setCurrentScreen(*debugMenu);
            delete demoScreen;
        };
    }));

    debugMenu->addMenuItem(MenuItem("Firmware Update", [this, settingsMenu, display]() {
        auto firmwareUpdateScreen = new FirmwareUpdateScreen(*display);
        this->setCurrentScreen(*firmwareUpdateScreen);
        firmwareUpdateScreen->backCallback = [this, firmwareUpdateScreen, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete firmwareUpdateScreen;
        };
    }));

    debugMenu->addMenuItem(MenuItem("Battery and Temperature", [this, display, debugMenu]() {
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

        display->updateDisplay();
        debugMenu->setFirstLoad();
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

    oldMenu->addMenuItem(MenuItem("Deep sleep", [display]() {
        log_w("Deep sleeping. Goodnight.");

        display->drawSingleLineOfText("Deep sleeping. Goodnight.", 0, 0);
        display->updateDisplay();
        delay(2000);
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
        auto demoScreen = new DemoScreen(*display, prefs_, this->power_);
        demoScreen->backCallback = [this, demoScreen, mainMenu]() {
            setCurrentScreen(*mainMenu);
            delete demoScreen;
        };
        this->setCurrentScreen(*demoScreen);
    } else if (currentReadable.has_value()) {
        // Check if we left-off on a readable and if so, reload that readable by default
        log_d("Attempting to find readable: %s", currentReadable.value().c_str());
        // If we previously had a readable loaded, show that same readable by default
        auto it =
            std::find_if(readables.begin(), readables.end(),
                         [&currentReadable](const Readable &x) { return currentReadable == x.id; });
        if (it != readables.end()) {
            auto readableScreen = createReadableScreen(&(*it));
            this->display_->setFont(this->display_->style.getFontFamily().readableFont);
            this->setCurrentScreen(*readableScreen);
            log_i("Resuming readable from last session: %s", (*it).name.c_str());
        } else {
            // Assume this readable has gone away
            prefs_.lastReadableId.remove();
        }
    }

    // This still has to be this way rather than else if because the currentReadable.has_value()
    // Could return true but if we don't find that readable, no screen will be set. I asssume this
    // would happen if the FS is updated underneath us to remove that readable.
    if (this->currentScreen_ == nullptr) {
        // If we've already connected to an input device, go straight
        // to the main menu. We will clean this up a bit moving forward
        // but for now it makes it clear when it's connected and when it
        // hasn't on boot.

        // By default, if we don't already have a different first screen ready, show the
        // calibration
        // screen

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

void RootUI::processCommand(RemoteCommand command) {
    if (command == FACTORY_DEFAULTS) {
        restoreFactoryDefaults();
    }
    ContainerScreen::processCommand(command);
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

void RootUI::restoreFactoryDefaults() {
    nvs_flash_erase();
    log_w("NVS cleared. Rebooting...");
    display_->drawSingleLineOfText(
        "NVS cleared! Resetting...",
        display_->xAbsolute(display_->xAbsoluteCenter() -
                            display_->getTextWidth("NVS cleared! Resetting...") / 2),
        display_->yAbsolute(display_->yAbsoluteCenter() - display_->getTextHeight() / 2));
    display_->updateDisplay();
    delay(2000);
    esp_restart();
}
