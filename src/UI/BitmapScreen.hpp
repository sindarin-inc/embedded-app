#include "UI/Screen.hpp"

#include "Renderers/Renderer.hpp"

class BitmapScreen : public Screen {
public:
    BitmapScreen(Renderer &renderer, const uint8_t *bitmap) : renderer(renderer), bitmap(bitmap) {}
    ~BitmapScreen() = default;

    void render();
    void process_command(RemoteCommand command);

protected:
    Renderer &renderer;
    const uint8_t *bitmap;
};
