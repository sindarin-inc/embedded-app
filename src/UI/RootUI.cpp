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

void RootUI::setup(DisplaySystem *display, OpticsSystem *optics, std::vector<Readable> &readables) {
    SHOW_HEAP();

    // TODO: Reload the current book / spot / menu

    // General renderer shared by menus and simple UIs
    this->renderer = new Renderer(optics, display);
    this->renderer->local_x_offset = prefs.localXOffset.get().value();
    this->renderer->local_y_offset = prefs.localYOffset.get().value();
    SIZE_OF(*this->renderer);

    auto main_menu = new PagedMenuScreen(*this->renderer, this->power);
    SIZE_OF(*main_menu);

    auto createReadableScreen = [this, optics, display,
                                 main_menu](Readable *readable) -> ReadableScreen * {
        // TODO: Would be better to not allocate / delete these all the time
        FileRenderer *bookRenderer = new FileRenderer(optics, display, *readable);
        bookRenderer->local_x_offset = this->prefs.localXOffset.get().value();
        bookRenderer->local_y_offset = this->prefs.localYOffset.get().value();
        this->prefs.lastReadableId.set(readable->id);
        ReadableScreen *readableScreen = new ReadableScreen(bookRenderer, *readable, this->prefs);
        readableScreen->back_callback = [this, bookRenderer, readableScreen, main_menu]() {
            delete bookRenderer;
            delete readableScreen;
            prefs.lastReadableId.remove();
            set_current_screen(*main_menu);
        };
        return readableScreen;
    };

    for (Readable &r : readables) {
        Readable *readable = &r;
        r.place = prefs.readablePlace.get(r.id);
        r.place_prev = prefs.readablePreviousPlace.get(r.id);
        main_menu->add_menu_item(MenuItem(r.name, [this, readable, createReadableScreen]() {
            auto readableScreen = createReadableScreen(readable);
            this->set_current_screen(*readableScreen);
        }));
    }
    SHOW_HEAP();

    // Set up pocket article menu
#if !defined(DISABLE_WIFI)
    main_menu->add_menu_item(MenuItem("Pocket Articles", [this, main_menu, optics, display]() {
        if (!WiFi.isConnected()) {
            log_w("Tried to load Pocket articles but WiFi not connected");
            this->renderer->render_max_box_with_text(String("WiFi Disconnected"));
            delay(2000);
            needs_rerender = true;
            return;
        }

        log_i("Loading top Pocket article");
        // TODO(John): I wanted to make a loading indicator for this but then realized I'll
        // need to add threading / tasks since the HTTP code is synchronous right now.
        this->renderer->render_max_box_with_text(String("Downloading..."));
        PocketAPI *pocket = PocketAPI::getInstance();
        pocket->setAccessToken(prefs.pocketApiToken.get().value().c_str());

        // Since we're reading this directly out of this from the closures, this needs to exist
        // until the closures no longer exist. That or we need to copy some strings.
        SpiRamJsonDocument *response;

        int code = pocket->get(&response);
        if (code != 200) {
            log_e("Pocket HTTP error: %d", code);
            this->renderer->render_max_box_with_text(String("HTTP error: ") + String(code));
            delay(2000);
            set_current_screen(*main_menu);
            return;
        }

        JsonObject list = (*response)["list"].as<JsonObject>();
        if (list.size() <= 0) {
            log_w("No Pocket articles found");
            this->renderer->render_max_box_with_text(String("No articles found"));
            delay(2000);
            set_current_screen(*main_menu);
            delete response;
            return;
        }

        log_i("Pocket /get request succeeded");
        auto pocket_menu = new MenuScreen(*renderer, this->power);

        for (JsonPair keyValue : list) {
            JsonObject article = keyValue.value().as<JsonObject>();
            const char *url = article["resolved_url"].as<const char *>();
            log_i("Got Pocket url %s", url);
            const char *title = article["resolved_title"].as<const char *>();
            pocket_menu->add_menu_item(MenuItem(title, [this, pocket_menu, title, url, pocket,
                                                        optics, display]() {
                log_d("Loading article %s (%s)", title, url);
                renderer->render_max_box_with_text(String("Downloading..."));
                SpiRamJsonDocument *response;
                auto code = pocket->text(url, &response);
                if (code != 200) {
                    if (code == -1) {
                        log_e("Pocket HTTP Error: %d (Article too long?)", code);
                        this->renderer->render_max_box_with_text(String("Error: Too long"));
                    } else {
                        log_e("Pocket HTTP Error: %d", code);
                        this->renderer->render_max_box_with_text(String("Pocket Error: ") + code);
                    }
                    delay(2000);
                    set_current_screen(*pocket_menu);
                    return;
                }
                const char *text = (*response)["article"].as<const char *>();
                log_i("Pocket text succeeded for %s", url);
                // TODO(john): We should eventually just save these to files and handle them
                // the same as any other readable. Probably not great to be copying this whole
                // article anyways.
                auto str = std::string(text);
                auto raw_renderer = new RawRenderer(optics, display, str);
                raw_renderer->local_x_offset = prefs.localXOffset.get().value();
                raw_renderer->local_y_offset = prefs.localYOffset.get().value();
                SIZE_OF(*raw_renderer);
                raw_renderer->render_page_with_box(0);

                auto raw_renderer_screen = new RawRendererScreen(raw_renderer);
                set_current_screen(*raw_renderer_screen);

                raw_renderer_screen->back_callback = [this, raw_renderer, raw_renderer_screen,
                                                      pocket_menu, response]() {
                    set_current_screen(*pocket_menu);
                    delete raw_renderer_screen;
                    delete raw_renderer;
                    delete response;
                };
            }));
        }

        set_current_screen(*pocket_menu);
        pocket_menu->back_callback = [this, main_menu, pocket_menu, response]() {
            set_current_screen(*main_menu);
            delete pocket_menu;
            delete response;
        };
    }));
#endif // DISABLE_WIFI

    // Settings menu
    auto settings_menu = new MenuScreen(*renderer, this->power);

    settings_menu->add_menu_item(
        MenuItem("Adjust Offset", [this, settings_menu, optics, display]() {
            log_d("Loading adjust offset");
            auto str = std::string();
            auto raw_renderer = new RawRenderer(optics, display, str);
            raw_renderer->local_x_offset = prefs.localXOffset.get().value();
            raw_renderer->local_y_offset = prefs.localYOffset.get().value();
            auto offset_adjust = new OffsetAdjustScreen(*raw_renderer, prefs, this->power);
            this->set_current_screen(*offset_adjust);
            log_d("setting up callback");
            offset_adjust->back_callback = [this, offset_adjust, settings_menu]() {
                set_current_screen(*settings_menu);
                delete offset_adjust;
                this->renderer->local_x_offset = prefs.localXOffset.get().value();
                this->renderer->local_y_offset = prefs.localYOffset.get().value();
            };
            log_d("set up callback");
        }));
    settings_menu->add_menu_item(MenuItem("Reset Prefs", [this, settings_menu]() {
        prefs.clear();
        log_w("Preferences cleared. Rebooting...");

        this->renderer->render_max_box_with_text("Preferences cleared! Resetting...");
        delay(2000);
        esp_restart();
    }));
    settings_menu->add_menu_item(
        MenuItem("Factory Defaults", [this, settings_menu]() { this->restoreFactoryDefaults(); }));
    settings_menu->add_menu_item(MenuItem("Loading Test", [this, settings_menu]() {
        auto loading_screen = new LoadingScreen(this->renderer);
        set_current_screen(*loading_screen);
        loading_screen->back_callback = [this, loading_screen, settings_menu]() {
            set_current_screen(*settings_menu);
            delete loading_screen;
        };
    }));
    settings_menu->add_menu_item(MenuItem("Test Bitmap Screen", [this, settings_menu]() {
        auto bitmap_screen = new BitmapScreen(*this->renderer, Logo_bits);
        set_current_screen(*bitmap_screen);
        bitmap_screen->back_callback = [this, bitmap_screen, settings_menu]() {
            set_current_screen(*settings_menu);
            delete bitmap_screen;
        };
    }));
    settings_menu->add_menu_item(MenuItem("Unpair TurnTouch", [this, settings_menu]() {
        prefs.pairedTurnTouchAddress.remove();
        log_w("Unpaired TurnTouch. Rebooting...");
        this->renderer->render_max_box_with_text("Unpaired TurnTouch! Resetting...");
        delay(2000);
        esp_restart();
    }));
    settings_menu->back_callback = [this, main_menu]() { set_current_screen(*main_menu); };

    main_menu->add_menu_item(
        MenuItem("**SETTINGS**", [this, settings_menu]() { set_current_screen(*settings_menu); }));

    // Check if we left-off on a readable and if so, reload that readable by default
    auto currentReadable = prefs.lastReadableId.get();
    if (currentReadable.has_value()) {
        log_d("Attempting to find readable: %s", currentReadable.value().c_str());
        // If we previously had a readable loaded, show that same readable by default
        std::vector<Readable>::iterator it =
            std::find_if(readables.begin(), readables.end(),
                         [&currentReadable](const Readable &x) { return currentReadable == x.id; });
        if (it != readables.end()) {
            auto readableScreen = createReadableScreen(&(*it));
            this->set_current_screen(*readableScreen);
            log_i("Resuming readable from last session: %s", (*it).name);
        } else {
            // Assume this readable has gone away
            prefs.lastReadableId.remove();
        }
    }

    // By default, if we don't already have a different first screen ready, show the calibration
    // screen
    if (this->current_screen == nullptr) {
        auto calibration_screen = new CalibrationScreen(*this->renderer);
        calibration_screen->back_callback = [this, calibration_screen, display, main_menu]() {
            this->renderer->render_max_box();
            delay(100);

            display->set_partial_refresh();

            set_current_screen(*main_menu);
            delete calibration_screen;
        };

        set_current_screen(*calibration_screen);
    }

    SHOW_HEAP();
}

void RootUI::process_command(RemoteCommand command) {
    if (command == FACTORY_DEFAULTS) {
        restoreFactoryDefaults();
    }
    ContainerScreen::process_command(command);
}

void RootUI::loop(RemoteCommand command) {
    if (this->power.getDepleted()) {
        // Force render immediately as we are about to shut off
        PowerDepletedScreen powerDepleted(*(this->renderer));
        set_current_screen(powerDepleted);
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
    renderer->render_max_box_with_text("NVS cleared! Resetting...");
    delay(2000);
    esp_restart();
}
