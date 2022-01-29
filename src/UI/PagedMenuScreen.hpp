#ifndef PAGED_MENU_SCREEN_H
#define PAGED_MENU_SCREEN_H

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/MenuScreen.hpp"
#include "sindarin-debug.h"

class PagedMenuScreen : public MenuScreen {

public:
    PagedMenuScreen(Power &power) : MenuScreen(power){};

    auto nextSelection() -> int override;
    auto prevSelection() -> int override;

    void render(DisplaySystem &display) override;

    void setBidirectional() { isBidirectional_ = true; }

protected:
    bool isBidirectional_ = false;
};

#endif
