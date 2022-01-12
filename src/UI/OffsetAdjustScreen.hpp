#include "Misc/Power.hpp"
#include "Renderers/RawRenderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class OffsetAdjustScreen : public Screen {
public:
    OffsetAdjustScreen(RawRenderer &renderer, PreferencesStore &prefs, Power &power)
        : renderer(renderer), prefs(prefs), power(power), xOffset(prefs.localXOffset.get().value()),
          yOffset(prefs.localYOffset.get().value()) {}

    void render();
    void process_command(RemoteCommand command);
    int xOffset;
    int yOffset;

protected:
    RawRenderer &renderer;
    PreferencesStore &prefs;
    Power &power;
};
