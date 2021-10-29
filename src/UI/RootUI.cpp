#include "RootUI.hpp"

#include <WiFi.h>

#include "Communications/PocketAPI.hpp"
#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Renderers/FileRenderer.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
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

    display->display_splash_screen();
    delay(500);

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
        SHOW_HEAP();
    }

    // TODO: Bring this back
    // Set up pocket article menu
    // main_menu->add_menu_item(MenuItem("Pocket Articles", [this, optics, display]() {
    //     if (!WiFi.isConnected()) {
    //         renderer->render_max_box_with_text(String("WiFi Disconnected"));
    //     } else {
    //         // TODO: This is a proof-of-concept. To implement this for real, we'll probably want
    //         // to implement loading a submenu of all your articles, but that should probably be
    //         // a part of a larger menu refactor.
    //         log_i("Loading top Pocket article");
    //         renderer->render_max_box_with_text(String("Downloading..."));
    //         PocketAPI *pocket = PocketAPI::getInstance();
    //         pocket->setAccessToken(prefs.pocket_api_token);
    //         const char *text;
    //         auto code = pocket->getFirstArticleText(&text);
    //         if (code == 200) {
    //             auto str = std::string(text);
    //             // TODO: Fix
    //             // raw_renderer = new RawRenderer(optics, display, str);
    //             raw_renderer->render_page_with_box(0);
    //         } else {
    //             renderer->render_max_box_with_text(String("Pocket Error: ") + code);
    //         }
    //     }
    // }));

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
        log_w("Preferences cleared\nRebooting...");

        renderer->render_max_box_with_text("Preferences cleared! Resetting...");
        delay(2000);

        resetFunc();
    }));
    settings_menu->back_callback = [this, main_menu]() { set_current_screen(*main_menu); };

    main_menu->add_menu_item(
        MenuItem("**SETTINGS**", [this, settings_menu]() { set_current_screen(*settings_menu); }));

    renderer->render_max_box();
    delay(250);

    display->set_partial_refresh();

    set_current_screen(*main_menu);

    SHOW_HEAP();
}

void RootUI::loop(RemoteCommand remote_command) {
    if (remote_command != RemoteCommand::NONE) {
        current_screen->process_command(remote_command);
    }
    current_screen->render();

    // NOTE(John): This does not work after coming back to the main menu and it is very mysterious
    // to me why that might be the case
    // current_screen->loop(remote_command);
}
