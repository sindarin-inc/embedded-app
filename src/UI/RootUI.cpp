#include "RootUI.hpp"

#include <nvs_flash.h>

#include "Bitmaps/Logo.hpp"
#include "Communications/HtmlCleaner.hpp"
#include "Communications/WiFi.hpp"
#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/CalibrationScreen.hpp"
#include "UI/DemoScreen.hpp"
#include "UI/LineSpacingScreen.hpp"
#include "UI/LoadingScreen.hpp"
#include "UI/LogoScreen.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PagedMenuScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
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

    auto mainMenu = new PagedMenuScreen(this->power_);

    mainMenu->setBidirectional();

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
            readableScreen->setLeftPopupButton("Delete", [this, readable, &fileSystem]() {
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
            display->renderMaxBoxWithText("WiFi Disconnected");
            delay(2000);
            needsRerender = true;
            return;
        }

        if (!prefs_.pocketApiToken.get().has_value()) {
            log_w("No Pocket API token found");
            display->renderMaxBoxWithText("No Pocket API token found");
            delay(2000);
            needsRerender = true;
            return;
        }

        log_i("Loading Pocket articles");
        // TODO(John): I wanted to make a loading indicator for this but then realized I'll
        // need to add threading / tasks since the HTTP code is synchronous right now.
        display->renderMaxBoxWithText("Downloading...");
        PocketAPI *pocket = PocketAPI::getInstance();
        pocket->setAccessToken(prefs_.pocketApiToken.get().value());

        // Since we're reading this directly out of this from the closures, this needs to exist
        // until the closures no longer exist. That or we need to copy some strings.
        SpiRamJsonDocument *response;

        int code = pocket->get(&response);
        if (code != 200) {
            log_e("Pocket HTTP error: %d", code);
            display->renderMaxBoxWithText(StringFormat("HTTP error: %d", code));
            delay(2000);
            setCurrentScreen(*mainMenu);
            return;
        }

        JsonObject list = (*response)["list"].as<JsonObject>();
        if (list.size() <= 0) {
            log_w("No Pocket articles found");
            display->renderMaxBoxWithText("No articles found");
            delay(2000);
            setCurrentScreen(*mainMenu);
            delete response;
            return;
        }

        log_i("Pocket /get request succeeded");
        auto pocketMenu = new MenuScreen(this->power_);

        for (JsonPair keyValue : list) {
            JsonObject article = keyValue.value().as<JsonObject>();
            const char *url = article["resolved_url"].as<const char *>();
            log_i("Got Pocket url %s", url);
            const char *title = article["resolved_title"].as<const char *>();
            pocketMenu->addMenuItem(MenuItem(title, [this, pocketMenu, title, url, pocket, display,
                                                     &fileSystem, createReadableScreen]() {
                log_d("Loading article %s (%s)", title, url);
                display->renderMaxBoxWithText("Downloading...");
                SpiRamJsonDocument *response;
                auto code = pocket->text(url, &response);
                if (code != 200) {
                    if (code == -1) {
                        log_e("Pocket HTTP Error: %d (Article too long?)", code);
                        display->renderMaxBoxWithText("Error: Too long");
                    } else {
                        log_e("Pocket HTTP Error: %d", code);
                        display->renderMaxBoxWithText(StringFormat("Pocket Error: %d", code));
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
                    display->renderMaxBoxWithText(StringFormat("FS error createDir"));
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

    // Settings menu
    auto settingsMenu = new MenuScreen(this->power_);

    settingsMenu->addMenuItem(MenuItem("Adjust Offset", [this, settingsMenu, display]() {
        log_d("Loading adjust offset");
        auto offsetAdjust = new OffsetAdjustScreen(*display, prefs_, this->power_);
        this->setCurrentScreen(*offsetAdjust);
        log_d("setting up callback");
        offsetAdjust->backCallback = [this, offsetAdjust, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete offsetAdjust;
        };
        log_d("set up callback");
    }));

    settingsMenu->addMenuItem(MenuItem("Resize Box", [this, settingsMenu, display]() {
        log_d("Loading resize box");
        auto resizeBox = new ResizeBoxScreen(*display, prefs_, this->power_);
        this->setCurrentScreen(*resizeBox);
        log_d("setting up callback");
        resizeBox->backCallback = [this, resizeBox, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete resizeBox;
        };
        log_d("set up callback");
    }));

    int inDemo = prefs_.demoMode.get().value();

    if (inDemo) {
        settingsMenu->addMenuItem(MenuItem("Turn off Demo Mode", [this, settingsMenu, display]() {
            log_d("Exiting demo");
            prefs_.demoMode.set(0);
            display->renderMaxBoxWithText("Demo mode off! Resetting...");
            delay(2000);
            esp_restart();
        }));
    } else {
        settingsMenu->addMenuItem(MenuItem("Turn on Demo Mode", [this, settingsMenu, display]() {
            log_d("Entering demo");
            prefs_.demoMode.set(1);
            display->renderMaxBoxWithText("Demo mode on! Resetting...");
            delay(2000);
            esp_restart();
        }));
    }

#if EINKET013TT1
    settingsMenu->addMenuItem(MenuItem("Set Vcom", [this, display, settingsMenu]() {
        auto vcomMenu = new MenuScreen(this->power_);
        auto *driver = (static_cast<DisplayEinkET013TT1 *>(display))->getDriver();
        vcomMenu->addMenuItem(MenuItem("0V", [this, vcomMenu, driver]() {
            driver->setVcom(0);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-1V", [this, vcomMenu, driver]() {
            driver->setVcom(-1000);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-2V", [this, vcomMenu, driver]() {
            driver->setVcom(-2000);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-3V", [this, vcomMenu, driver]() {
            driver->setVcom(-3000);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-4V", [this, vcomMenu, driver]() {
            driver->setVcom(-4000);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-5V", [this, vcomMenu, driver]() {
            driver->setVcom(-5000);
            vcomMenu->needsRerender = true;
        }));
        vcomMenu->addMenuItem(MenuItem("-5.6V", [this, vcomMenu, driver]() {
            driver->setVcom(-5600);
            vcomMenu->needsRerender = true;
        }));
        setCurrentScreen(*vcomMenu);
        vcomMenu->backCallback = [this, vcomMenu, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete vcomMenu;
        };
    }));
#endif

#if EINKET013TT1
    settingsMenu->addMenuItem(MenuItem("Set Vcom Precise", [this, display, settingsMenu]() {
        auto vcomPreciseScreen = new VcomAdjustScreen(*display, this->prefs_, this->power_);
        this->display_->clearDisplay();
        setCurrentScreen(*vcomPreciseScreen);
        vcomPreciseScreen->backCallback = [this, vcomPreciseScreen, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete vcomPreciseScreen;
        };
    }));
#endif

    settingsMenu->addMenuItem(MenuItem("Set Font", [this, display, settingsMenu]() {
        auto fontMenu = new MenuScreen(this->power_);

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
            prefs_.font.set(PREFS_DEFAULT_FONT);
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
        fontMenu->backCallback = [this, fontMenu, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete fontMenu;
        };
    }));

    settingsMenu->addMenuItem(MenuItem("Set Line Spacing", [this, settingsMenu, display]() {
        auto lineSpacingScreen = new LineSpacingScreen(*display, prefs_);
        this->setCurrentScreen(*lineSpacingScreen);
        lineSpacingScreen->backCallback = [this, lineSpacingScreen, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete lineSpacingScreen;
        };
    }));

    settingsMenu->addMenuItem(MenuItem("Calibration Screen", [this, settingsMenu, display]() {
        auto calibrationScreen = new CalibrationScreen();
        this->display_->clearDisplay();
        this->setCurrentScreen(*calibrationScreen);
        calibrationScreen->backCallback = [this, calibrationScreen, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete calibrationScreen;
        };
    }));

    settingsMenu->addMenuItem(MenuItem("Optics Test", [this, settingsMenu, display]() {
        auto opticsTestScreen = new CalibrationScreen();
        opticsTestScreen->setOpticsTest();
        this->display_->clearDisplay();
        this->setCurrentScreen(*opticsTestScreen);
        opticsTestScreen->backCallback = [this, opticsTestScreen, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete opticsTestScreen;
        };
    }));

    settingsMenu->addMenuItem(MenuItem("Deep sleep", [this, display]() {
        log_w("Deep sleeping. Goodnight.");

        display->renderMaxBoxWithText("Deep sleeping. Goodnight.");
        delay(2000);
        esp_deep_sleep_start();
    }));

    settingsMenu->addMenuItem(MenuItem("Reset Prefs", [this, display]() {
        prefs_.clear();
        log_w("Preferences cleared. Rebooting...");

        display->renderMaxBoxWithText("Preferences cleared! Resetting...");
        delay(2000);
        esp_restart();
    }));

    settingsMenu->addMenuItem(
        MenuItem("Factory Defaults", [this, settingsMenu]() { this->restoreFactoryDefaults(); }));
    settingsMenu->addMenuItem(MenuItem("Loading Test", [this, display, settingsMenu]() {
        auto loadingScreen = new LoadingScreen();
        setCurrentScreen(*loadingScreen);
        loadingScreen->backCallback = [this, loadingScreen, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete loadingScreen;
        };
    }));

    settingsMenu->addMenuItem(MenuItem("Demo Screen", [this, settingsMenu, display]() {
        auto demoScreen = new DemoScreen(*display, prefs_, this->power_);
        this->setCurrentScreen(*demoScreen);
        demoScreen->backCallback = [this, demoScreen, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete demoScreen;
        };
    }));

    settingsMenu->addMenuItem(MenuItem("Test Bitmap Screen", [this, display, settingsMenu]() {
        auto bitmapScreen = new BitmapScreen(LOGO_BITS, LOGO_WIDTH, LOGO_HEIGHT);
        setCurrentScreen(*bitmapScreen);
        bitmapScreen->backCallback = [this, bitmapScreen, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete bitmapScreen;
        };
    }));

#if USE_TURN_TOUCH_REMOTE
    settingsMenu->addMenuItem(MenuItem("Unpair TurnTouch", [this, display]() {
        prefs_.pairedTurnTouchAddress.remove();
        log_w("Unpaired TurnTouch. Rebooting...");
        display->renderMaxBoxWithText("Unpaired TurnTouch! Resetting...");
        delay(2000);
        esp_restart();
    }));
#endif

    settingsMenu->backCallback = [this, mainMenu]() {
        this->display_->clearDisplay();
        setCurrentScreen(*mainMenu);
    };

    mainMenu->addMenuItem(MenuItem("**SETTINGS**", [this, settingsMenu]() {
        this->display_->clearDisplay();
        setCurrentScreen(*settingsMenu);
    }));

    inDemo = prefs_.demoMode.get().value();

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
        auto calibrationScreen = new LogoScreen();
        calibrationScreen->backCallback = [this, calibrationScreen, display, mainMenu]() {
            display->clearScreen();
            delay(100);

            setCurrentScreen(*mainMenu);
            delete calibrationScreen;
        };
        setCurrentScreen(*calibrationScreen);
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
    display_->renderMaxBoxWithText("NVS cleared! Resetting...");
    delay(2000);
    esp_restart();
}
