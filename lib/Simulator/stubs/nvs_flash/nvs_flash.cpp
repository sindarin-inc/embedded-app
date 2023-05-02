#include "nvs_flash.h"

#include <filesystem>

void nvs_flash_erase() {
    // delete sim prefs file
    std::filesystem::remove("simprefs.ini");
}