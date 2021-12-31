#include "RootUI.hpp"

#include <WiFi.h>
#include <nvs_flash.h>

#include "Bitmaps/Logo.hpp"
#include "Communications/PocketAPI.hpp"
#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Renderers/RawRenderer.hpp"
#include "UI/CalibrationScreen.hpp"
#include "UI/LoadingScreen.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/RawRendererScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "sindarin-debug.h"

void RootUI::setup(DisplaySystem *display, OpticsSystem *optics, std::vector<Readable> &readables) {
    SHOW_HEAP();

    // TODO: Reload the current book / spot / menu

    // General renderer shared by menus and simple UIs
    renderer = new Renderer(optics, display);
    renderer->local_x_offset = prefs.local_x_offset;
    renderer->local_y_offset = prefs.local_y_offset;
    SIZE_OF(*renderer);

    auto main_menu = new MenuScreen(*renderer);
    SIZE_OF(*main_menu);

    for (Readable &r : readables) {
        String name = String(r.id) + "-p";
        String name_prev = String(r.id) + "-pr-p";
        Readable *readable = &r;
        main_menu->add_menu_item(MenuItem(r.name, [this, readable, optics, display, main_menu]() {
            // TODO: Would be better to not allocate / delete these all the time
            FileRenderer *book_renderer = new FileRenderer(optics, display, *readable);
            book_renderer->local_x_offset = prefs.local_x_offset;
            book_renderer->local_y_offset = prefs.local_y_offset;
            ReadableScreen *readable_screen = new ReadableScreen(book_renderer, *readable, prefs);
            set_current_screen(*readable_screen);
            readable_screen->back_callback = [this, book_renderer, readable_screen, main_menu]() {
                delete book_renderer;
                delete readable_screen;
                set_current_screen(*main_menu);
            };
        }));
        r.place = prefs.prefs.getULong(name.c_str());
        r.place_prev = prefs.prefs.getULong(name_prev.c_str());
    }
    SHOW_HEAP();

    // Set up pocket article menu
#if !defined(DISABLE_WIFI)
    main_menu->add_menu_item(MenuItem("Pocket Articles", [this, main_menu, optics, display]() {
        if (!WiFi.isConnected()) {
            log_w("Tried to load Pocket articles but WiFi not connected");
            renderer->render_max_box_with_text(String("WiFi Disconnected"));
            delay(2000);
            needs_rerender = true;
            return;
        }

        log_i("Loading top Pocket article");
        // TODO(John): I wanted to make a loading indicator for this but then realized I'll
        // need to add threading / tasks since the HTTP code is synchronous right now.
        renderer->render_max_box_with_text(String("Downloading..."));
        PocketAPI *pocket = PocketAPI::getInstance();
        pocket->setAccessToken(prefs.pocket_api_token);

        // Since we're reading this directly out of this from the closures, this needs to exist
        // until the closures no longer exist. That or we need to copy some strings.
        SpiRamJsonDocument *response;

        int code = pocket->get(&response);
        if (code != 200) {
            log_e("Pocket HTTP error: %d", code);
            renderer->render_max_box_with_text(String("HTTP error: ") + String(code));
            delay(2000);
            set_current_screen(*main_menu);
            return;
        }

        JsonObject list = (*response)["list"].as<JsonObject>();
        if (list.size() <= 0) {
            log_w("No Pocket articles found");
            renderer->render_max_box_with_text(String("No articles found"));
            delay(2000);
            set_current_screen(*main_menu);
            delete response;
            return;
        }

        log_i("Pocket /get request succeeded");
        auto pocket_menu = new MenuScreen(*renderer);

        for (JsonPair keyValue : list) {
            JsonObject article = keyValue.value().as<JsonObject>();
            const char *url = article["resolved_url"].as<const char *>();
            log_i("Got Pocket url %s", url);
            const char *title = article["resolved_title"].as<const char *>();
            pocket_menu->add_menu_item(
                MenuItem(title, [this, pocket_menu, title, url, pocket, optics, display]() {
                    log_d("Loading article %s (%s)", title, url);
                    renderer->render_max_box_with_text(String("Downloading..."));
                    SpiRamJsonDocument *response;
                    auto code = pocket->text(url, &response);
                    if (code != 200) {
                        if (code == -1) {
                            log_e("Pocket HTTP Error: %d (Article too long?)", code);
                            renderer->render_max_box_with_text(String("Error: Too long"));
                        } else {
                            log_e("Pocket HTTP Error: %d", code);
                            renderer->render_max_box_with_text(String("Pocket Error: ") + code);
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
                    raw_renderer->local_x_offset = prefs.local_x_offset;
                    raw_renderer->local_y_offset = prefs.local_y_offset;
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
    auto settings_menu = new MenuScreen(*renderer);

    settings_menu->add_menu_item(MenuItem("Adjust Offset", [this, settings_menu]() {
        log_d("Loading adjust offset");
        auto offset_adjust = new OffsetAdjustScreen(*renderer, prefs);
        set_current_screen(*offset_adjust);
        log_d("setting up callback");
        offset_adjust->back_callback = [this, offset_adjust, settings_menu]() {
            set_current_screen(*settings_menu);
            delete offset_adjust;
        };
        log_d("set up callback");
    }));
    settings_menu->add_menu_item(MenuItem("Reset Prefs", [this, settings_menu]() {
        prefs.prefs.clear();
        log_w("Preferences cleared. Rebooting...");

        renderer->render_max_box_with_text("Preferences cleared! Resetting...");
        delay(2000);
        esp_restart();
    }));
    settings_menu->add_menu_item(MenuItem("Reset NVS", [this, settings_menu]() {
        nvs_flash_erase();
        log_w("NVS cleared. Rebooting...");

        renderer->render_max_box_with_text("NVS cleared! Resetting...");
        delay(2000);
        esp_restart();
    }));
    settings_menu->add_menu_item(MenuItem("Loading Test", [this, settings_menu]() {
        auto loading_screen = new LoadingScreen(renderer);
        set_current_screen(*loading_screen);
        loading_screen->back_callback = [this, loading_screen, settings_menu]() {
            set_current_screen(*settings_menu);
            delete loading_screen;
        };
    }));
    settings_menu->add_menu_item(MenuItem("Test Bitmap Screen", [this, settings_menu]() {
        auto bitmap_screen = new BitmapScreen(*renderer, Logo_bits);
        set_current_screen(*bitmap_screen);
        bitmap_screen->back_callback = [this, bitmap_screen, settings_menu]() {
            set_current_screen(*settings_menu);
            delete bitmap_screen;
        };
    }));
    settings_menu->back_callback = [this, main_menu]() { set_current_screen(*main_menu); };

    main_menu->add_menu_item(
        MenuItem("**SETTINGS**", [this, settings_menu]() { set_current_screen(*settings_menu); }));

    auto calibration_screen = new CalibrationScreen(*renderer);
    calibration_screen->back_callback = [this, &calibration_screen, display, main_menu]() {
        renderer->render_max_box();
        delay(100);

        display->set_partial_refresh();

        set_current_screen(*main_menu);
        delete calibration_screen;
        calibration_screen = nullptr;
    };

    set_current_screen(*calibration_screen);

    SHOW_HEAP();
}
