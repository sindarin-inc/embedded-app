
#include <Renderer/Renderer.hpp>

#include "UI/BitmapScreen.hpp"
#include "UI/Bitmaps/Battery_Low.hpp"
#include "UI/Screen.hpp"

class PowerDepletedScreen : public BitmapScreen {
public:
    PowerDepletedScreen() : BitmapScreen(BATTERY_LOW_BITS, BATTERY_LOW_WIDTH, BATTERY_LOW_HEIGHT) {}
    ~PowerDepletedScreen() override = default;
};
