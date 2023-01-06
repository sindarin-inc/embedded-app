#ifndef PAGED_MENU_SCREEN_H
#define PAGED_MENU_SCREEN_H

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/MenuScreen.hpp"
#include "sindarin-debug.h"

class PagedMenuScreen : public MenuScreen {

public:
    PagedMenuScreen(Power &power) : MenuScreen(power){};

    auto nextSelection(bool skipLast = false) -> bool override;
    auto prevSelection(bool skipLast = false) -> bool override;

    void render(DisplaySystem &display) override;

protected:
    void processCommand(RemoteCommand command) override;
};

#endif
