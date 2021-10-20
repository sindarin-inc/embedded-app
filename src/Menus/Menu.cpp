#include "Menu.hpp"

#include <WiFi.h>

#include "Communications/PocketAPI.hpp"
#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

#define NUM_LINES_PER_MENU_PAGE 5

void Menu::setup(DisplaySystem *display, OpticsSystem *optics) {
    SHOW_HEAP();

    this->my_display = display;
    this->my_optics = optics;

    log_i("Optics: %d Display %d", (int)this->my_optics, (int)this->my_display);

    box_renderer = new Renderer(my_optics, my_display);
    SIZE_OF(*box_renderer);

    // Load the preferences
    local_x_offset = prefs.local_x_offset;
    local_y_offset = prefs.local_y_offset;
    if (prefs.area_in_browsing == 0) {
        // Default area for browsing should be MAIN MENU which is readables length + 2 (length is
        // SETTINGS MENU)
        area_in_browsing = readables.size() + 2;
    } else {
        area_in_browsing = prefs.area_in_browsing;
    }
    DEBUG_VAR(area_in_browsing);
    DEBUG_LN(" was the area read in");

    my_display->display_splash_screen();

    delay(500);

    DEBUG_VAR(area_in_browsing);
    DEBUG_LN(" was the area read in");

    main_menu = new MenuSystem();
    SIZE_OF(*main_menu); // 1208

    SHOW_HEAP();
    for (Readable &r : readables) {
        String name = String(r.id) + "-p";
        String name_prev = String(r.id) + "-pr-p";
        main_menu->add_menu_item(r.name);
        r.place = prefs.prefs.getULong(name.c_str());
        r.place_prev = prefs.prefs.getULong(name_prev.c_str());
        SHOW_HEAP();
    }

    if (area_in_browsing < readables.size()) {
        book_renderer = new FileRenderer(my_optics, my_display, readables[area_in_browsing]);
        SIZE_OF(*book_renderer);
    }

    main_menu->add_menu_item("Pocket Articles");
    main_menu->add_menu_item("**SETTINGS**");

    box_renderer->render_max_box(local_x_offset, local_y_offset);
    delay(250);
    update_box = 1;

    my_display->set_partial_refresh();

    SHOW_HEAP();
}

void Menu::loop(bool got_a_release, int code_from_release) {
    String menu_string;
    String bat_life;
    String total_to_display;

    if (update_box) {
        DEBUG_LN("UPDATING THE BOX!");

        // MAIN MENU is readables length + 1 (length is SETTINGS MENU)
        if (raw_renderer != nullptr) {
            if (raw_renderer != nullptr && raw_renderer->get_read_mode() == PAGE_READ) {
                DEBUG_LN(" Rendering some remote text in READ MODE");
                new_place =
                    raw_renderer->render_page_with_box(raw_place, local_x_offset, local_y_offset);
                DEBUG_VAR(new_place);
                DEBUG_LN(" is the new place in the text");
            }
        } else if (area_in_browsing == readables.size() + 1) {
            menu_string =
                main_menu->get_menu_range_as_string(cur_menu_page * NUM_LINES_PER_MENU_PAGE + 1,
                                                    cur_menu_page * NUM_LINES_PER_MENU_PAGE + 5);
            DEBUG_LN("UPDATING THE MENU WITH CURR MENU_PAGE: ");
            DEBUG_VAR(cur_menu_page);
            DEBUG_LN(" is the current menu page");
            bat_life = get_battery_life();
            total_to_display = menu_string + String("\n\n") + bat_life;
            box_renderer->render_max_box_with_text(local_x_offset, local_y_offset,
                                                   total_to_display);
            // Partial refresh doesn't seem to work on both eInk displays on very first render, only
            // hits one? -cojo
            if (!hasPartialRefreshed) {
                hasPartialRefreshed = true;
                box_renderer->render_max_box_with_text(local_x_offset, local_y_offset,
                                                       total_to_display);
            }
        } else if (area_in_browsing == readables.size()) // Pocket API menu
        {
            if (!WiFi.isConnected()) {
                box_renderer->render_max_box_with_text(local_x_offset, local_y_offset,
                                                       String("WiFi Disconnected"));
            } else {
                // TODO: This is a proof-of-concept. To implement this for real, we'll probably want
                // to implement loading a submenu of all your articles, but that should probably be
                // a part of a larger menu refactor.
                log_i("Loading top Pocket article");
                box_renderer->render_max_box_with_text(local_x_offset, local_y_offset,
                                                       String("Downloading..."));
                PocketAPI *pocket = PocketAPI::getInstance();
                pocket->setAccessToken(prefs.pocket_api_token);
                const char *text;
                auto code = pocket->getFirstArticleText(&text);
                if (code == 200) {
                    auto str = std::string(text);
                    raw_renderer = new RawRenderer(my_optics, my_display, str);
                    raw_renderer->render_page_with_box(raw_place, local_x_offset, local_y_offset);
                } else {
                    box_renderer->render_max_box_with_text(local_x_offset, local_y_offset,
                                                           String("Pocket Error: ") + code);
                }
            }
        } else if (area_in_browsing == readables.size() + 1) {
            menu_string = "Press arrows to move\nbox around to your\nliking. Then press\nenter "
                          "when done!\nHave a nice day.\n";
            bat_life = get_battery_life();
            total_to_display = menu_string + String("\n\n") + bat_life;
            box_renderer->render_max_box_with_text(local_x_offset, local_y_offset,
                                                   total_to_display);
        } else {
            // All the contents are the rest of the menu items
            if (book_renderer != nullptr && book_renderer->get_read_mode() == PAGE_READ) {
                DEBUG_LN(" Rendering some text in READ MODE");
                new_place = book_renderer->render_page_with_box(readables[area_in_browsing].place,
                                                                local_x_offset, local_y_offset);
                DEBUG_VAR(new_place);
                DEBUG_LN(" is the new place in the text");
            }
        }
        update_box = 0;
    }

    if (raw_renderer != nullptr)
        raw_renderer->render_continue_screed(local_x_offset, local_x_offset);
    else if (area_in_browsing < readables.size())
        readables[area_in_browsing].place_screed =
            book_renderer->render_continue_screed(local_x_offset, local_y_offset);

    auto select = [this]() {
        // Check against readables.size() because it is == SETTINGS_MENU
        if (area_in_browsing == readables.size()) {
            prefs.setLocalXOffset(local_x_offset);
            prefs.setLocalYOffset(local_y_offset);
            DEBUG_LN("Saving off x and y offset to preferences");
            DEBUG_VAR(local_x_offset);
            DEBUG_LN(" is the x offset");
            DEBUG_VAR(local_y_offset);
            DEBUG_LN(" is the y offset");
            // Set to MAIN_MENU (readables.size() + 1)
            area_in_browsing = readables.size() + 1;
            prefs.setAreaInBrowsing(area_in_browsing);
            update_box = 1;
        } else if (area_in_browsing == readables.size() + 1) {
            // CAN FIX THIS NOW TO JUST DO ONE ASSIGNMENT BASED ON THE
            // RETURN RESULT OF THE MENU!
            area_in_browsing = main_menu->get_curr_selection();
            if (book_renderer != nullptr) {
                delete book_renderer;
                book_renderer = nullptr;
            }
            // Only create the FileRenderer if we didn't select the SETTINGS MENU
            if (area_in_browsing != readables.size()) {
                book_renderer =
                    new FileRenderer(my_optics, my_display, readables[area_in_browsing]);
                SIZE_OF(*book_renderer);
            }
            prefs.setAreaInBrowsing(area_in_browsing);
            update_box = 1;
        }
    };

    auto back = [this]() {
        if (book_renderer != nullptr && book_renderer->get_read_mode() == SCREED_READ) {
            String prefs_prev_name = String(readables[area_in_browsing].id) + "-pr-p";
            String prefs_name = String(readables[area_in_browsing].id) + "-p";
            readables[area_in_browsing].place_prev = readables[area_in_browsing].place;
            readables[area_in_browsing].place = readables[area_in_browsing].place_screed;
            prefs.prefs.putULong(prefs_name.c_str(), readables[area_in_browsing].place);
            prefs.prefs.putULong(prefs_prev_name.c_str(), readables[area_in_browsing].place_prev);
        }

        // Set to MAIN_MENU (readables.size() + 1)
        area_in_browsing = readables.size() + 1;
        prefs.setAreaInBrowsing(area_in_browsing);
        update_box = 1;
        if (book_renderer != nullptr) {
            delete book_renderer;
            book_renderer = nullptr;
        }
        if (raw_renderer != nullptr) {
            delete raw_renderer;
            raw_renderer = nullptr;
            raw_place = 0;
        }
    };

    if (got_a_release) {
        switch (code_from_release) {
        case SETUP:
            prefs.prefs.clear();
            DEBUG_LN("CLEARED PREFERENCES!");
            DEBUG_LN("RESETTING BOARD BY SEG FAULT!");
            resetFunc();
            break;
        case STOP:
            DEBUG_LN("RESETTING BOARD BY SEG FAULT!");
            resetFunc();
            break;
        case CHANNEL_MINUS:
            // TODO: Check bounds
            // if (start_x != 0) {
            if (raw_renderer != nullptr) {
                raw_place = raw_place_prev;
            }
            // Check against readables.size() because it is == SETTINGS_MENU
            else if (area_in_browsing == readables.size())
                local_x_offset--;
            else if (area_in_browsing < readables.size()) {
                String prefs_name = String(readables[area_in_browsing].id) + "-pr-p";
                readables[area_in_browsing].place = book_renderer->get_previous_place();
                prefs.prefs.putULong(prefs_name.c_str(), readables[area_in_browsing].place);
            }
            update_box = 1;
            //}
            break;
        case CHANNEL_PLUS:
            // TODO:
            // Check to be sure we don't go off the end of the screen
            // if (end_x != my_display->get_x()) {
            if (raw_renderer != nullptr) {
                raw_place_prev = raw_place;
                raw_place = new_place;
            }
            // Check against readables.size() because it is == SETTINGS_MENU
            else if (area_in_browsing == readables.size())
                local_x_offset++;
            else if (area_in_browsing < readables.size()) {
                String prefs_prev_name = String(readables[area_in_browsing].id) + "-pr-p";
                String prefs_name = String(readables[area_in_browsing].id) + "-p";

                readables[area_in_browsing].place_prev = readables[area_in_browsing].place;
                readables[area_in_browsing].place = new_place;
                prefs.prefs.putULong(prefs_name.c_str(), readables[area_in_browsing].place);
                prefs.prefs.putULong(prefs_prev_name.c_str(),
                                     readables[area_in_browsing].place_prev);
            }
            update_box = 1;
            //}
            select();
            break;
        case PREV:
            // TODO: Check bounds
            // if (start_y != 0) {
            if (raw_renderer != nullptr) {
                if (raw_renderer->get_read_mode() == SCREED_READ) raw_renderer->increase_speed();
            }
            // Check against readables.size() because it is == SETTINGS_MENU
            else if (area_in_browsing == readables.size())
                local_y_offset--;
            // Check against readables.size() + 1 because it is == MAIN_MENU
            else if (area_in_browsing == readables.size() + 1) {
                if (main_menu->prev_selection()) {
                    // Hit a page boundary -- update the page!
                    if ((main_menu->get_curr_selection() % NUM_LINES_PER_MENU_PAGE) ==
                        NUM_LINES_PER_MENU_PAGE - 1) {
                        cur_menu_page--;
                    }
                }
            } else {
                if (book_renderer->get_read_mode() == SCREED_READ) book_renderer->increase_speed();
            }
            update_box = 1;
            //}
            back();
            break;
        case NEXT:
            // TODO: Check to be sure we don't go off the end of the screen
            // if (end_y != my_display->get_y()) {
            if (raw_renderer != nullptr) {
                if (raw_renderer->get_read_mode() == SCREED_READ) raw_renderer->decrease_speed();
            }
            // Check against readables.size() because it is == SETTINGS_MENU
            else if (area_in_browsing == readables.size())
                local_y_offset++;
            // Check against readables.size() + 1 because it is == MAIN_MENU
            else if (area_in_browsing == readables.size() + 1) {
                if (main_menu->next_selection()) {
                    // Hit a page boundary -- update the page!
                    if (main_menu->get_curr_selection() % NUM_LINES_PER_MENU_PAGE == 0) {
                        cur_menu_page++;
                    }
                }
            } else {
                if (book_renderer->get_read_mode() == SCREED_READ) book_renderer->decrease_speed();
            }
            update_box = 1;
            //}
            break;
        case ENTER:
            select();
            break;
        case BACK:
            back();
            break;
        case NOP_BUTTON:
            if (raw_renderer != nullptr) {
                raw_renderer->flip_read_mode();
            } else if (area_in_browsing < readables.size()) {
                if (book_renderer->get_read_mode() == SCREED_READ) {
                    String prefs_prev_name = String(readables[area_in_browsing].id) + "-pr-p";
                    String prefs_name = String(readables[area_in_browsing].id) + "-p";
                    readables[area_in_browsing].place_prev = readables[area_in_browsing].place;
                    readables[area_in_browsing].place = readables[area_in_browsing].place_screed;
                    prefs.prefs.putULong(prefs_name.c_str(), readables[area_in_browsing].place);
                    prefs.prefs.putULong(prefs_prev_name.c_str(),
                                         readables[area_in_browsing].place_prev);
                }
                book_renderer->flip_read_mode();
            }
            update_box = 1;
            break;
        default:
            break;
        }
    }
}