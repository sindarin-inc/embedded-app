
#include "Renderers/Renderer.hpp"
#include "UI/Bitmaps/Battery_Low.hpp"
#include "UI/Screen.hpp"

class PowerDepletedScreen : public BitmapScreen {
public:
    PowerDepletedScreen(Renderer &renderer) : BitmapScreen(renderer, Battery_Low_bits) {}
    ~PowerDepletedScreen() = default;
};
