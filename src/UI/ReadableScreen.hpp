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
        ledcSetup(PWMChannel, PWMFreq, PWMResolution);
        /* Attach the LED PWM Channel to the GPIO Pin */
        ledcAttachPin(LEDPin, PWMChannel);
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

    int ledLevel;

    unsigned long newPlace_ = 0;

    std::function<void(void)> leftButtonCallback_;

    const int LEDPin = RED_LED; /* GPIO16 */

    int dutyCycle;
    /* Setting PWM Properties */
    const int PWMFreq = 5000; /* 5 KHz */
    const int PWMChannel = 0;
    const int PWMResolution = 10;
    const int PWMIncrement = 128;
    const int MAX_DUTY_CYCLE = (int)(pow(2, PWMResolution) - 1);
};
