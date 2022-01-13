#ifndef PAGED_MENU_SCREEN_H
#define PAGED_MENU_SCREEN_H

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/MenuScreen.hpp"
#include "sindarin-debug.h"

class PagedMenuScreen : public MenuScreen {

public:
    PagedMenuScreen(Renderer &renderer, Power &power) : MenuScreen(renderer, power){};

    auto next_selection() -> int override;
    auto prev_selection() -> int override;

    void render() override;
};

#endif