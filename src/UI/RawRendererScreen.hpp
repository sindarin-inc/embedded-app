#include "Renderers/RawRenderer.hpp"
#include "UI/Screen.hpp"

// TODO(John) Unify this with ReadableScreen maybe with a common renderer superclass perhaps
// `TextRenderer`
class RawRendererScreen : public Screen {
public:
    RawRendererScreen(RawRenderer *renderer) : renderer_(renderer) {}
    ~RawRendererScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    RawRenderer *renderer_;
    unsigned long newPlace_ = 0, prevPlace_ = 0;
};
