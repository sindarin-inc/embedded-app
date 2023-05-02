#include "FactoryReset.hpp"

#include <nvs_flash.h>

#include "Misc/SolMorse.hpp"
#include "sindarin-debug.h"

void FactoryReset(PreferencesStore &prefs, FileSystem &fs, PowerManager &pm, bool reboot) {
    log_i("performing factory reset");
    prefs.clear();
    fs.deleteAll();
    // delete entire nvs partition
    log_i("erasing all nvs");
    nvs_flash_erase();

    if (reboot) {
        pm.releaseLED();
        BlinkSol();
        pm.reboot();
    }
}