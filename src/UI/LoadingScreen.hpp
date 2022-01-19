#include "Displays/DisplaySystem.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class LoadingScreen : public Screen {
public:
    LoadingScreen(Renderer *renderer) : renderer_(renderer) {}
    void render() override;
    void processCommand(RemoteCommand command) override;

protected:
    Renderer *renderer_;
    unsigned long timer_ = 0;
    int state_ = 0;
};
