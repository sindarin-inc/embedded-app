#include "Displays/DisplaySystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class LoadingScreen : public Screen {
public:
    LoadingScreen() = default;
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    unsigned long timer_ = 0;
    int state_ = 0;
};
