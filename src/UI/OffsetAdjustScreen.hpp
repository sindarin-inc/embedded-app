#include "Misc/Power.hpp"
#include "Renderers/RawRenderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class OffsetAdjustScreen : public Screen {
public:
    OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

    int xOffset;
    int yOffset;

protected:
    DisplaySystem &display_;
    RawRenderer renderer_;
    PreferencesStore &prefs_;
    Power &power_;
};
