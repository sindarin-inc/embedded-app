#include "RootUI.hpp"

#include <WiFi.h>
#include <nvs_flash.h>

#include "Bitmaps/Logo.hpp"
#include "Communications/PocketAPI.hpp"
#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Inputs/TTRemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Renderers/RawRenderer.hpp"
#include "UI/CalibrationScreen.hpp"
#include "UI/LoadingScreen.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PagedMenuScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "UI/RawRendererScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "sindarin-debug.h"

#define LAST_READABLE_ID "rui-readable"

void RootUI::setup(DisplaySystem *display, std::vector<Readable> &readables) {
    SHOW_HEAP();

    // TODO: Reload the current book / spot / menu

    // General renderer shared by menus and simple UIs
    this->renderer_ = new Renderer(display);
    this->renderer_->localXOffset = prefs_.localXOffset.get().value();
    this->renderer_->localYOffset = prefs_.localYOffset.get().value();
    SIZE_OF(*this->renderer_);

    auto mainMenu = new PagedMenuScreen(*this->renderer_, this->power_);
    SIZE_OF(*mainMenu);

    auto createReadableScreen = [this, display, mainMenu](Readable *readable) -> ReadableScreen * {
        // TODO: Would be better to not allocate / delete these all the time
        auto *bookRenderer = new FileRenderer(display, *readable);
        bookRenderer->localXOffset = this->prefs_.localXOffset.get().value();
        bookRenderer->localYOffset = this->prefs_.localYOffset.get().value();
        this->prefs_.lastReadableId.set(readable->id);
        auto *readableScreen = new ReadableScreen(bookRenderer, *readable, this->prefs_);
        readableScreen->backCallback = [this, bookRenderer, readableScreen, mainMenu]() {
            delete bookRenderer;
            delete readableScreen;
            prefs_.lastReadableId.remove();
            setCurrentScreen(*mainMenu);
        };
        return readableScreen;
    };

    for (Readable &r : readables) {
        Readable *readable = &r;
        r.place = prefs_.readablePlace.get(r.id);
        r.placePrev = prefs_.readablePreviousPlace.get(r.id);
        mainMenu->addMenuItem(MenuItem(r.name, [this, readable, createReadableScreen]() {
            auto readableScreen = createReadableScreen(readable);
            this->setCurrentScreen(*readableScreen);
        }));
    }
    SHOW_HEAP();

    // Set up pocket article menu
#if !defined(DISABLE_WIFI)
    mainMenu->addMenuItem(MenuItem("Pocket Articles", [this, mainMenu, display]() {
        if (!WiFi.isConnected()) {
            log_w("Tried to load Pocket articles but WiFi not connected");
            this->renderer_->renderMaxBoxWithText(String("WiFi Disconnected"));
            delay(2000);
            needsRerender = true;
            return;
        }

        log_i("Loading top Pocket article");
        // TODO(John): I wanted to make a loading indicator for this but then realized I'll
        // need to add threading / tasks since the HTTP code is synchronous right now.
        this->renderer_->renderMaxBoxWithText(String("Downloading..."));
        PocketAPI *pocket = PocketAPI::getInstance();
        pocket->setAccessToken(prefs_.pocketApiToken.get().value().c_str());

        // Since we're reading this directly out of this from the closures, this needs to exist
        // until the closures no longer exist. That or we need to copy some strings.
        SpiRamJsonDocument *response;

        int code = pocket->get(&response);
        if (code != 200) {
            log_e("Pocket HTTP error: %d", code);
            this->renderer_->renderMaxBoxWithText(String("HTTP error: ") + String(code));
            delay(2000);
            setCurrentScreen(*mainMenu);
            return;
        }

        JsonObject list = (*response)["list"].as<JsonObject>();
        if (list.size() <= 0) {
            log_w("No Pocket articles found");
            this->renderer_->renderMaxBoxWithText(String("No articles found"));
            delay(2000);
            setCurrentScreen(*mainMenu);
            delete response;
            return;
        }

        log_i("Pocket /get request succeeded");
        auto pocketMenu = new MenuScreen(*renderer_, this->power_);

        for (JsonPair keyValue : list) {
            JsonObject article = keyValue.value().as<JsonObject>();
            const char *url = article["resolved_url"].as<const char *>();
            log_i("Got Pocket url %s", url);
            const char *title = article["resolved_title"].as<const char *>();
            pocketMenu->addMenuItem(
                MenuItem(title, [this, pocketMenu, title, url, pocket, display]() {
                    log_d("Loading article %s (%s)", title, url);
                    renderer_->renderMaxBoxWithText(String("Downloading..."));
                    SpiRamJsonDocument *response;
                    auto code = pocket->text(url, &response);
                    if (code != 200) {
                        if (code == -1) {
                            log_e("Pocket HTTP Error: %d (Article too long?)", code);
                            this->renderer_->renderMaxBoxWithText(String("Error: Too long"));
                        } else {
                            log_e("Pocket HTTP Error: %d", code);
                            this->renderer_->renderMaxBoxWithText(String("Pocket Error: ") + code);
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
                    auto str = std::string(text);
                    auto rawRenderer = new RawRenderer(display, str);
                    rawRenderer->localXOffset = prefs_.localXOffset.get().value();
                    rawRenderer->localYOffset = prefs_.localYOffset.get().value();
                    SIZE_OF(*rawRenderer);
                    rawRenderer->renderPageWithBox(0);

                    auto rawRendererScreen = new RawRendererScreen(rawRenderer);
                    setCurrentScreen(*rawRendererScreen);

                    rawRendererScreen->backCallback = [this, rawRenderer, rawRendererScreen,
                                                       pocketMenu, response]() {
                        setCurrentScreen(*pocketMenu);
                        delete rawRendererScreen;
                        delete rawRenderer;
                        delete response;
                    };
                }));
        }

        setCurrentScreen(*pocketMenu);
        pocketMenu->backCallback = [this, mainMenu, pocketMenu, response]() {
            setCurrentScreen(*mainMenu);
            delete pocketMenu;
            delete response;
        };
    }));
#endif // DISABLE_WIFI

    // Settings menu
    auto settingsMenu = new MenuScreen(*renderer_, this->power_);

    settingsMenu->addMenuItem(MenuItem("Adjust Offset", [this, settingsMenu, display]() {
        log_d("Loading adjust offset");
        auto str = std::string();
        auto rawRenderer = new RawRenderer(display, str);
        rawRenderer->localXOffset = prefs_.localXOffset.get().value();
        rawRenderer->localYOffset = prefs_.localYOffset.get().value();
        auto offsetAdjust = new OffsetAdjustScreen(*rawRenderer, prefs_, this->power_);
        this->setCurrentScreen(*offsetAdjust);
        log_d("setting up callback");
        offsetAdjust->backCallback = [this, offsetAdjust, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete offsetAdjust;
            this->renderer_->localXOffset = prefs_.localXOffset.get().value();
            this->renderer_->localYOffset = prefs_.localYOffset.get().value();
        };
        log_d("set up callback");
    }));
    settingsMenu->addMenuItem(MenuItem("Reset Prefs", [this, settingsMenu]() {
        prefs_.clear();
        log_w("Preferences cleared. Rebooting...");

        this->renderer_->renderMaxBoxWithText("Preferences cleared! Resetting...");
        delay(2000);
        esp_restart();
    }));
    settingsMenu->addMenuItem(
        MenuItem("Factory Defaults", [this, settingsMenu]() { this->restoreFactoryDefaults(); }));
    settingsMenu->addMenuItem(MenuItem("Loading Test", [this, settingsMenu]() {
        auto loadingScreen = new LoadingScreen(this->renderer_);
        setCurrentScreen(*loadingScreen);
        loadingScreen->backCallback = [this, loadingScreen, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete loadingScreen;
        };
    }));
    settingsMenu->addMenuItem(MenuItem("Test Bitmap Screen", [this, settingsMenu]() {
        auto bitmapScreen = new BitmapScreen(*this->renderer_, LOGO_BITS);
        setCurrentScreen(*bitmapScreen);
        bitmapScreen->backCallback = [this, bitmapScreen, settingsMenu]() {
            setCurrentScreen(*settingsMenu);
            delete bitmapScreen;
        };
    }));
    settingsMenu->addMenuItem(MenuItem("Unpair TurnTouch", [this, settingsMenu]() {
        prefs_.pairedTurnTouchAddress.remove();
        log_w("Unpaired TurnTouch. Rebooting...");
        this->renderer_->renderMaxBoxWithText("Unpaired TurnTouch! Resetting...");
        delay(2000);
        esp_restart();
    }));
    settingsMenu->backCallback = [this, mainMenu]() { setCurrentScreen(*mainMenu); };

    mainMenu->addMenuItem(
        MenuItem("**SETTINGS**", [this, settingsMenu]() { setCurrentScreen(*settingsMenu); }));

    // Check if we left-off on a readable and if so, reload that readable by default
    auto currentReadable = prefs_.lastReadableId.get();
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
        auto calibrationScreen = new CalibrationScreen(*this->renderer_);
        calibrationScreen->backCallback = [this, calibrationScreen, display, mainMenu]() {
            this->renderer_->renderMaxBox();
            delay(100);

            display->setPartialRefresh();

            setCurrentScreen(*mainMenu);
            delete calibrationScreen;
        };

        setCurrentScreen(*calibrationScreen);
    }

    SHOW_HEAP();
}

void RootUI::processCommand(RemoteCommand command) {
    if (command == FACTORY_DEFAULTS) {
        restoreFactoryDefaults();
    }
    ContainerScreen::processCommand(command);
}

void RootUI::loop(RemoteCommand command) {
    if (this->power_.getDepleted()) {
        // Force render immediately as we are about to shut off
        PowerDepletedScreen powerDepleted(*(this->renderer_));
        setCurrentScreen(powerDepleted);
        this->render();
        // Wait 5 seconds to be able to see the message
        delay(5000);
        // Unless wake sources are defined, nothing can wake us up. Goodnight.
        esp_deep_sleep_start();
    }
    Screen::loop(command);
}

void RootUI::restoreFactoryDefaults() {
    nvs_flash_erase();
    log_w("NVS cleared. Rebooting...");
    renderer_->renderMaxBoxWithText("NVS cleared! Resetting...");
    delay(2000);
    esp_restart();
}
