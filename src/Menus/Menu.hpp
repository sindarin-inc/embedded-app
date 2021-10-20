#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Menus/MenuSystem.hpp"
#include "Optics/OpticsSystem.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Renderers/RawRenderer.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"

class Menu {
public:
    Menu(PreferencesStore &prefs) : prefs(prefs) {}
    void setup(DisplaySystem *my_display, OpticsSystem *my_optics);
    void loop(bool got_a_release, int code_from_release);

    // TODO: Not ideal to expose these directly but I'm trying to keep
    // the core code as unmodified as possible for this first pass
    int update_box = 1;
    RawRenderer *raw_renderer = nullptr;
    std::vector<Readable> readables;

protected:
    MenuSystem *main_menu;
    DisplaySystem *my_display;
    // Whether to redisplay the box or not
    unsigned long new_place = 0;
    int cur_menu_page = 0;
    int local_x_offset = 0;
    int local_y_offset = 0;
    int area_in_browsing = 0;
    Renderer *box_renderer;
    OpticsSystem *my_optics;
    bool hasPartialRefreshed = false;
    FileRenderer *book_renderer = nullptr;
    // Raw renderer was put in place for BLE uploaded data test -cojo
    unsigned long raw_place = 0, raw_place_prev = 0;
    PreferencesStore &prefs;
};
