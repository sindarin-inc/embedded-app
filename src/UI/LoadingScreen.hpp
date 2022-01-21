#include "Displays/DisplaySystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class LoadingScreen : public Screen {
public:
    LoadingScreen(DisplaySystem &display) : display_(display) {}
    void render() override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    unsigned long timer_ = 0;
    int state_ = 0;
};
