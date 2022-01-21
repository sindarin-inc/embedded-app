
#include "Renderers/Renderer.hpp"
#include "UI/Bitmaps/Battery_Low.hpp"
#include "UI/Screen.hpp"

class PowerDepletedScreen : public BitmapScreen {
public:
    PowerDepletedScreen() : BitmapScreen(BATTERY_LOW_BITS) {}
    ~PowerDepletedScreen() override = default;
};
