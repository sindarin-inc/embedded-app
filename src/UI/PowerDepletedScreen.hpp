
#include "Renderers/Renderer.hpp"
#include "UI/Screen.hpp"

class PowerDepletedScreen : public Screen {
public:
    PowerDepletedScreen(Renderer &renderer) : renderer(renderer) {}

    void render();
    void process_command(RemoteCommand command);

protected:
    Renderer &renderer;
};
