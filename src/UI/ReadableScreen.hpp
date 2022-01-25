#include "Contents/Readable.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/CalibrationScreen.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PagedMenuScreen.hpp"
#include "UI/PopupMenuScreen.hpp"
#include "UI/Screen.hpp"

class ReadableScreen : public ContainerScreen {
public:
    ReadableScreen(FileRenderer *fileRenderer, Readable &readable, PreferencesStore &prefs,
                   Power &power)
        : fileRenderer_(fileRenderer), readable_(readable), prefs_(prefs), power_(power) {
        popup_ = new PopupMenuScreen(power, fileRenderer);
        currentScreen_ = this;
    }

    ~ReadableScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    FileRenderer *fileRenderer_;
    Readable &readable_;
    PreferencesStore &prefs_;
    Power &power_;

    PopupMenuScreen *popup_;

    bool inPopup_ = false;

    unsigned long newPlace_ = 0;
};
