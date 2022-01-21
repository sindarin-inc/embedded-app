#include "Displays/DisplaySystem.hpp"
#include "UI/Screen.hpp"

class BitmapScreen : public Screen {
public:
    BitmapScreen(const uint8_t *bitmap) : bitmap_(bitmap) {}
    ~BitmapScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    const uint8_t *bitmap_;
};
