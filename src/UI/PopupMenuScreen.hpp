#ifndef POPUP_MENU_SYSTEM_H
#define POPUP_MENU_SYSTEM_H

#include <utility>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class PopupMenuScreen : public Screen {

public:
    PopupMenuScreen(Power &power, FileRenderer *fileRenderer)
        : power_(power), fileRenderer_(fileRenderer), leftButtonTitle_("Menu"){};

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override{};

    void setLeftButtonTitle(const std::string &title) { leftButtonTitle_ = title; };

protected:
    Power &power_;
    FileRenderer *fileRenderer_;
    std::string leftButtonTitle_;
};

#endif
