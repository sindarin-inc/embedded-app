#include "Renderers/RawRenderer.hpp"
#include "UI/Screen.hpp"

// TODO(John) Unify this with ReadableScreen maybe with a common renderer superclass perhaps
// `TextRenderer`
class RawRendererScreen : public Screen {
public:
    RawRendererScreen(RawRenderer *renderer) : renderer(renderer) {}
    ~RawRendererScreen() = default;

    void render() override;
    void process_command(RemoteCommand command) override;

protected:
    RawRenderer *renderer;
    unsigned long new_place = 0, prev_place = 0;
};
