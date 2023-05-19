
#include "UI/BitmapScreen.hpp"
#include "UI/Bitmaps/OverTemp.hpp"

class OverTempScreen : public BitmapScreen {
public:
    OverTempScreen() : BitmapScreen(OVER_TEMP_BITS, sizeof(OVER_TEMP_BITS)) {}
    ~OverTempScreen() override = default;
};
