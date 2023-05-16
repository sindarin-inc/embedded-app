
#include "Misc/SecretCode.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/Bitmaps/BatteryChargingLarge.hpp"
#include "UI/Screen.hpp"

class ChargingScreen : public BitmapScreen {
public:
    ChargingScreen(PreferencesStore &prefs)
        : BitmapScreen(BATTERY_CHARGING_LARGE_BITS, BatteryChargingLargeWidth,
                       BatteryChargingLargeHeight) {
        secretCode_.setCallback([&prefs]() {
            log_d("Enabling use while charging");
            prefs.allowUsageWhileCharging.set(true);
            PowerManager::getInstance().reboot();
        });
    }
    ~ChargingScreen() override = default;

    void processCommand(RemoteCommand command) override { secretCode_.processCommand(command); }

protected:
    SecretCode secretCode_;
};
