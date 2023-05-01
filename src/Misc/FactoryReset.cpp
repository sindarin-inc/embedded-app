#include "FactoryReset.hpp"

#include <nvs_flash.h>

#include "Misc/SolMorse.hpp"
#include "sindarin-debug.h"

void FactoryReset(PreferencesStore &prefs, FileSystem &fs, PowerManager &pm) {
    log_i("performing factory reset");
    prefs.clear();
    fs.deleteAll();
    // delete entire nvs partition
#if !SIMULATOR
    log_i("erasing all nvs");
    nvs_flash_erase();
#endif
    pm.releaseLED();
    blinkSOL();
    pm.reboot();
}