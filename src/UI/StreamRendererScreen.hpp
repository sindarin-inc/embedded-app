#include "Renderers/StreamRenderer.hpp"
#include "UI/Screen.hpp"

// TODO(John) Unify this with ReadableScreen maybe with a common renderer superclass perhaps
// `TextRenderer`
class StreamRendererScreen : public Screen {
public:
    StreamRendererScreen(StreamRenderer *renderer) : renderer_(renderer) {}
    ~StreamRendererScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    StreamRenderer *renderer_;
    unsigned long newPlace_ = 0, prevPlace_ = 0;
};
