#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class OffsetAdjustScreen : public Screen {
public:
    OffsetAdjustScreen(Renderer &renderer, PreferencesStore &prefs)
        : renderer(renderer), prefs(prefs) {}

    void render();
    void process_command(RemoteCommand command);

protected:
    Renderer &renderer;
    PreferencesStore &prefs;
};
