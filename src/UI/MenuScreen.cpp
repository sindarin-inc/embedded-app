#include "UI/MenuScreen.hpp"

#include <utility>

#define NUM_LINES_PER_MENU_PAGE 8

void MenuScreen::add_menu_item(const MenuItem &new_item) {
    menu_items[num_menu_items] = std::move(new_item);
    num_menu_items++;
}

void MenuScreen::reset_menu_items() { num_menu_items = 0; }

auto MenuScreen::get_num_items() -> int { return num_menu_items; }

void MenuScreen::set_curr_selection(int new_selection) { curr_selection = new_selection; }

auto MenuScreen::get_curr_selection() -> int { return curr_selection; }

auto MenuScreen::get_curr_item() -> MenuItem { return menu_items[curr_selection]; }

// Returns 1 if the selection was updated, 0 otherwise
auto MenuScreen::next_selection() -> int {
    int old_sel = curr_selection;
    curr_selection++;
    // Don't go further than the last item in the menu
    if (curr_selection >= num_menu_items) curr_selection = num_menu_items - 1;
    auto diff = curr_selection - old_sel;
    if (diff) {
        // Hit a page boundary -- update the page!
        if (get_curr_selection() % NUM_LINES_PER_MENU_PAGE == 0) {
            cur_menu_page++;
        }
    }
    return diff;
}

// Returns 1 if the selection was updated, 0 otherwise
auto MenuScreen::prev_selection() -> int {
    int old_sel = curr_selection;
    curr_selection--;
    // Don't go before the first menu item
    if (curr_selection < 0) curr_selection = 0;
    auto diff = old_sel - curr_selection;
    if (diff) {
        // Hit a page boundary -- update the page!
        if ((get_curr_selection() % NUM_LINES_PER_MENU_PAGE) == NUM_LINES_PER_MENU_PAGE - 1) {
            cur_menu_page--;
        }
    };
    return diff;
}

// Includes the name of the item and it's number preceding it for easy display
// Offset by two spaces
auto MenuScreen::get_item_title_at(int index) -> String {
    String all;
    assert(index < num_menu_items);
    // Add the indicator for where we are in the menu based on the current selection
    if (index == curr_selection)
        all = " ->";
    else
        all = "   ";
    all = all + " " + menu_items[index].title;
    return all;
}

auto MenuScreen::get_item_at(int index) -> MenuItem {
    assert(index < num_menu_items);
    return menu_items[index];
}

auto MenuScreen::get_whole_menu_as_string() -> String {
    return get_menu_range_as_string(0, num_menu_items - 1);
}

auto MenuScreen::get_menu_range_as_string(int start_item, int end_item) -> String {
    int i;
    String complete = "";
    // If the client gives us an end_item past the end of the range, just go to the
    // end only
    for (i = start_item - 1; i < end_item && i < num_menu_items; i++) {
        complete = complete + get_item_title_at(i);
        complete = complete + "\n";
    }
    return complete;
}

auto MenuScreen::get_paginated_menu_string() -> String {
    String menu_string = get_menu_range_as_string(cur_menu_page * NUM_LINES_PER_MENU_PAGE + 1,
                                                  cur_menu_page * NUM_LINES_PER_MENU_PAGE + 5);
    return menu_string;
}

void MenuScreen::render() {
    if (!needs_rerender) {
        return;
    }
    log_i("Menuscreen render (selected = %s)", get_curr_item().title.c_str());
    String menu_string = get_paginated_menu_string();
    String bat_life = power.getBatteryLifeString();
    String total_to_display = menu_string + String("\n\n") + bat_life;
    renderer.render_max_box_with_text(total_to_display);
    needs_rerender = false;
}

void MenuScreen::process_command(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
        needs_rerender = prev_selection();
        break;
    case RemoteCommand::NEXT:
        needs_rerender = next_selection();
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::CHANNEL_PLUS:
        if (get_curr_item().callback != nullptr) {
            get_curr_item().callback();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        if (back_callback != nullptr) {
            back_callback();
        }
        break;
    default:
        break;
    }
}
