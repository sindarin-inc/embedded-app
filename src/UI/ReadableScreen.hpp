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

class ReadableScreen : public ContainerScreen {
public:
    ReadableScreen(RendererStream *stream, DisplaySystem &display, Readable &readable,
                   PreferencesStore &prefs, Power &power)
        : readable_(readable), prefs_(prefs), power_(power), renderer_(display),
          popup_(power, renderer_) {
        currentScreen_ = this;
        renderer_.setStream(stream);
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

    unsigned long newPlace_ = 0;

    std::function<void(void)> leftButtonCallback_;
};
