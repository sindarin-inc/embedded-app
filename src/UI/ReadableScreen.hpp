#include <utility>

#include "Contents/Readable.hpp"
#include "Misc/Power.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/PopupMenuScreen.hpp"
#include "UI/Screen.hpp"
#include "config.h"

class ReadableScreen : public ContainerScreen {
public:
    ReadableScreen(RendererStream *stream, DisplaySystem &display, Readable &readable,
                   PreferencesStore &prefs, Power &power)
        : renderer_(display), readable_(readable), prefs_(prefs),
          popup_(power, renderer_, prefs_, readable_) {
        currentScreen_ = this;
        renderer_.setStream(stream);
        popup_.backCallback = [this]() { this->setCurrentScreen(*this); };
    }

    ~ReadableScreen() override = default;

    void setLeftPopupButton(const std::string &title, std::function<void(void)> callback) {
        popup_.setLeftPopupButton(title, std::move(callback));
    }

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    Renderer renderer_;
    Readable &readable_;
    PreferencesStore &prefs_;

    PopupMenuScreen popup_;

    unsigned long newPlace_ = 0;

private:
    void incrementReadingTime();
    uint32_t readingStartTime_;
};
