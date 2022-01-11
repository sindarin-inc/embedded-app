#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

#define MAX_NUM_MENU_ITEMS 10

class MenuItem {
public:
    MenuItem(){};
    MenuItem(const String &title) : title(title){};
    MenuItem(const String &title, std::function<void()> callback)
        : title(title), callback(callback){};

    String title;
    // Called when this menu item is selected
    std::function<void()> callback;
};

class MenuScreen : public Screen {

public:
    MenuScreen(Renderer &renderer, Power &power) : renderer(renderer), power(power) {
        num_menu_items = 0;
    };

    void add_menu_item(const MenuItem &new_item);

    void reset_menu_items();

    auto get_num_items() -> int;

    void set_curr_selection(int new_selection);

    auto get_curr_selection() -> int;

    auto get_curr_item() -> MenuItem;

    // Returns 1 if the selection was updated, 0 otherwise
    // Helps client to know if it needs to refresh (at beginning or end of menu)
    auto next_selection() -> int;
    auto prev_selection() -> int;

    // Includes the name of the item and it's number preceding it for easy display
    // Offset by two spaces
    auto get_item_title_at(int index) -> String;

    auto get_item_at(int index) -> MenuItem;

    // Easy \n separated list of the items in the menu that can simply be passed
    // to a display. May want to handle this in a Renderer? But for now, this
    // is simple enough.
    auto get_whole_menu_as_string() -> String;

    // Allows the client to determine how many items from the menu it wants and
    // to generate a string accordingly.
    // Note it's up to the client to figure out what to do if the current
    // selection is outside of this range.
    auto get_menu_range_as_string(int start_item, int end_item) -> String;

    auto get_paginated_menu_string() -> String;

    void render();

    void process_command(RemoteCommand command);

protected:
    int num_menu_items = 0;
    std::array<MenuItem, MAX_NUM_MENU_ITEMS> menu_items;
    int curr_selection = 0;
    int cur_menu_page = 0;

    Renderer &renderer;
    Power &power;
};

#endif
