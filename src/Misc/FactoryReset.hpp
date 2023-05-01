#pragma once

#include "Misc/PowerManager.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

void FactoryReset(PreferencesStore &prefs, FileSystem &fs, PowerManager &pm, bool reboot);