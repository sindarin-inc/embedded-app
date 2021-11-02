#include "Displays/DisplaySystem.hpp"
#include "Optics/OpticsSystem.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class LoadingScreen : public Screen {
public:
    LoadingScreen(Renderer *renderer) : renderer(renderer) {}
    void render();
    void process_command(RemoteCommand command);

protected:
    Renderer *renderer;
    unsigned long timer = 0;
    int state = 0;
};
