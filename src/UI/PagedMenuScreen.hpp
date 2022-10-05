#ifndef PAGED_MENU_SCREEN_H
#define PAGED_MENU_SCREEN_H

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/MenuScreen.hpp"
#include "sindarin-debug.h"

class PagedMenuScreen : public MenuScreen {

public:
    PagedMenuScreen(Power &power) : MenuScreen(power){};

    void nextSelection(bool skipLast = false) override;
    void prevSelection(bool skipLast = false) override;

    void render(DisplaySystem &display) override;

    void setBidirectional() { isBidirectional_ = true; }

protected:
    bool isBidirectional_ = false;
    void processCommand(RemoteCommand command) override;
};

#endif
