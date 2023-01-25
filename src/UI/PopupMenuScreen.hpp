#ifndef POPUP_MENU_SYSTEM_H
#define POPUP_MENU_SYSTEM_H

#include <utility>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Bitmaps/Battery_Bolt.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/StackView.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/Layout/View.hpp"
#include "UI/LightingAdjustScreen.hpp"
#include "UI/ReadableSeekScreen.hpp"
#include "sindarin-debug.h"

class PopupMenuScreen : public ContainerScreen, public EinkView {

public:
    PopupMenuScreen(DisplaySystem &display, Power &power, Renderer &renderer,
                    PreferencesStore &prefs, Readable &readable,
                    LightingController &lightingController)
        : EinkView(display), prefs_(prefs), power_(power), renderer_(renderer), readable_(readable),
          leftButtonTitle_("Brightness"), lightingAdjustScreen_(display, prefs, lightingController),
          readableSeekScreen_(prefs, readable, renderer_), batteryTextView_(display, ""),
          batteryIconView_(display, boltBits, bolt_width, bolt_height),
          placeInReadableView_(display, "") {
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

    auto layout(LayoutContext &context, Size size) -> Size override;

    void drawInRect(Rect rect) override;

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
    Readable &readable_;
    std::string leftButtonTitle_;
    LightingAdjustScreen lightingAdjustScreen_;

    ReadableSeekScreen readableSeekScreen_;
    std::function<void(void)> leftButtonCallback_;

    SingleLineTextView batteryTextView_;

    IconView batteryIconView_;

    SingleLineTextView placeInReadableView_;

    // For later :)
    // std::shared_ptr<SingleLineTextView> clockView_;

    int arrowWidth_ = 5;
    int arrowHeight_ = 10;
    int arrowTextSpacing_ = 5;
};

#endif
