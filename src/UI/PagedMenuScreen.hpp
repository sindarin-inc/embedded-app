#ifndef PAGED_MENU_SCREEN_H
#define PAGED_MENU_SCREEN_H

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/MenuScreen.hpp"
#include "sindarin-debug.h"

class PagedMenuScreen : public MenuScreen {

public:
    PagedMenuScreen(DisplaySystem &display, Power &power) : MenuScreen(display, power){};

    auto nextSelection() -> int override;
    auto prevSelection() -> int override;

    void render() override;
};

#endif