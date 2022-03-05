#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/CalibrationScreen.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PagedMenuScreen.hpp"
#include "UI/PopupMenuScreen.hpp"
#include "UI/Screen.hpp"
#include "config.h"

class ReadableScreen : public ContainerScreen {
public:
    ReadableScreen(RendererStream *stream, DisplaySystem &display, Readable &readable,
                   PreferencesStore &prefs, Power &power)
        : readable_(readable), prefs_(prefs), power_(power), renderer_(display),
          popup_(power, renderer_) {
        currentScreen_ = this;
        renderer_.setStream(stream);
        ledcSetup(pwmChannel_, pwmFreq_, pwmResolution_);
        /* Attach the LED PWM Channel to the GPIO Pin */
        ledcAttachPin(ledPin_, pwmChannel_);
    }

    ~ReadableScreen() override = default;

    void setLeftPopupButton(const std::string &title, std::function<void(void)> callback);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    Renderer renderer_;
    Readable &readable_;
    PreferencesStore &prefs_;
    Power &power_;

    PopupMenuScreen popup_;

    bool inPopup_ = false;
    bool inLighting_ = false;

    int ledLevel_;

    unsigned long newPlace_ = 0;

    std::function<void(void)> leftButtonCallback_;

    const int ledPin_ = RED_LED; /* GPIO16 */

    int dutyCycle_;
    /* Setting PWM Properties */
    const int pwmFreq_ = 5000; /* 5 KHz */
    const int pwmChannel_ = 0;
    const int pwmResolution_ = 10;
    const int pwmIncrement_ = 128;
    const int maxDutyCycle_ = (int)(pow(2, pwmResolution_) - 1);
};
