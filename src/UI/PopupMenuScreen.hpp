#ifndef POPUP_MENU_SYSTEM_H
#define POPUP_MENU_SYSTEM_H

#include <utility>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/LightingAdjustScreen.hpp"
#include "UI/ReadableSeekScreen.hpp"
#include "sindarin-debug.h"

class PopupMenuScreen : public ContainerScreen {

public:
    PopupMenuScreen(Power &power, Renderer &renderer, PreferencesStore &prefs, Readable &readable)
        : prefs_(prefs), power_(power), renderer_(renderer), leftButtonTitle_("Lighting"),
          lightingAdjustScreen_(prefs), readableSeekScreen_(prefs, readable, renderer_) {
        currentScreen_ = this;

        lightingAdjustScreen_.backCallback = [this]() {
            this->needsRerender = true;
            this->setCurrentScreen(*this);
        };
        readableSeekScreen_.backCallback = [this]() { this->setCurrentScreen(*this); };
        leftButtonCallback_ = [this]() {
            lightingAdjustScreen_.firstLoad = true;
            this->setCurrentScreen(lightingAdjustScreen_);
        };
    };

    void drawBattery(DisplaySystem &display, int x, int y, int percentage);

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    void setLeftPopupButton(const std::string &title, std::function<void(void)> callback) {
        leftButtonTitle_ = title;
        leftButtonCallback_ = std::move(callback);
    }

protected:
    PreferencesStore &prefs_;
    Power &power_;
    Renderer &renderer_;
    std::string leftButtonTitle_;
    LightingAdjustScreen lightingAdjustScreen_;

    ReadableSeekScreen readableSeekScreen_;
    std::function<void(void)> leftButtonCallback_;

    int arrowWidth_ = 5;
    int arrowHeight_ = 10;
    int arrowTextSpacing_ = 5;
};

#endif
