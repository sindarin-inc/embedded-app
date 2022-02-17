#include "RootUI.hpp"

#include <nvs_flash.h>

#include "Bitmaps/Logo.hpp"
#include "Communications/HtmlCleaner.hpp"
#include "Communications/WiFi.hpp"
#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Renderers/StreamRenderer.hpp"
#include "UI/CalibrationScreen.hpp"
#include "UI/LoadingScreen.hpp"
#include "UI/LogoScreen.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PagedMenuScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/StreamRendererScreen.hpp"
#include "config.h"
#include "sindarin-debug.h"

#if ENABLE_POCKET
#include "Communications/PocketAPI.hpp"
#endif

#if USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
#endif

#define LAST_READABLE_ID "rui-readable"

void RootUI::setup(DisplaySystem *display, std::vector<Readable> &readables, bool inputConnected,
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
        auto *bookRenderer = new FileRenderer(*display, *readable, fileSystem);
        this->prefs_.lastReadableId.set(readable->id);
        auto *readableScreen =
            new ReadableScreen(bookRenderer, *readable, this->prefs_, this->power_);
        // When switching to a readable from a menu, we should clear the display
        // to make the whites really white and the blacks really black.
        bookRenderer->getDisplay()->clearDisplay();
        readableScreen->backCallback = [this, bookRenderer, readableScreen, mainMenu]() {
#if !DISABLE_WIFI
            ConnectWiFiIfCredentialsExist();
#endif
            // When leaving a readable to go back to the main menu, do a full clear
            // of the display.
            bookRenderer->getDisplay()->clearDisplay();
            delete bookRenderer;
            delete readableScreen;
            prefs_.lastReadableId.remove();
            setCurrentScreen(*mainMenu);
        };
        return readableScreen;
    };

    auto currentReadable = prefs_.lastReadableId.get();
    for (Readable &r : readables) {
        Readable *readable = &r;
        r.place = prefs_.readablePlace.get(r.id);
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
    mainMenu->addMenuItem(MenuItem("Pocket Articles", [this, mainMenu, display]() {
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

        log_i("Loading top Pocket article");
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
            pocketMenu->addMenuItem(
                MenuItem(title, [this, pocketMenu, title, url, pocket, display]() {
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
                    // TODO(john): We should eventually just save these to files and handle them
                    // the same as any other readable. Probably not great to be copying this whole
                    // article anyways.
                    auto streamRenderer = new StreamRenderer(*display);
                    std::string str = text;
                    std::stringstream out;
                    StripHtml(str, out);
                    streamRenderer->setStream(out);

                    SIZE_OF(*streamRenderer);
                    streamRenderer->renderPage(0);

                    auto streamRendererScreen = new StreamRendererScreen(streamRenderer);
                    setCurrentScreen(*streamRendererScreen);

                    streamRendererScreen->backCallback =
                        [this, streamRenderer, streamRendererScreen, pocketMenu, response]() {
                            setCurrentScreen(*pocketMenu);
                            delete streamRendererScreen;
                            delete streamRenderer;
                            delete response;
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

    // Check if we left-off on a readable and if so, reload that readable by default
    if (currentReadable.has_value()) {
        log_d("Attempting to find readable: %s", currentReadable.value().c_str());
        // If we previously had a readable loaded, show that same readable by default
        auto it =
            std::find_if(readables.begin(), readables.end(),
                         [&currentReadable](const Readable &x) { return currentReadable == x.id; });
        if (it != readables.end()) {
            auto readableScreen = createReadableScreen(&(*it));
            this->setCurrentScreen(*readableScreen);
            log_i("Resuming readable from last session: %s", (*it).name);
        } else {
            // Assume this readable has gone away
            prefs_.lastReadableId.remove();
        }
    }

    // By default, if we don't already have a different first screen ready, show the calibration
    // screen
    if (this->currentScreen_ == nullptr) {
        auto calibrationScreen = new LogoScreen();
        calibrationScreen->backCallback = [this, calibrationScreen, display, mainMenu]() {
            display->clearScreen();
            delay(100);

            setCurrentScreen(*mainMenu);
            delete calibrationScreen;
        };

        // If we've already connected to an input device, go straight
        // to the main menu. We will clean this up a bit moving forward
        // but for now it makes it clear when it's connected and when it
        // hasn't on boot.
        if (!inputConnected) {
            setCurrentScreen(*calibrationScreen);
        } else {
            setCurrentScreen(*mainMenu);
        }
    }

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
