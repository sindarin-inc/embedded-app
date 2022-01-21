#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp"

class BitmapScreen : public Screen {
public:
    BitmapScreen(DisplaySystem &display, const uint8_t *bitmap)
        : display_(display), bitmap_(bitmap) {}
    ~BitmapScreen() override = default;

    void render() override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    const uint8_t *bitmap_;
};
