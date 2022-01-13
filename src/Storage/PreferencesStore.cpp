#include "PreferencesStore.hpp"

#include "sindarin-debug.h"

void PreferencesStore::begin() { prefs.begin("sindarin-app", false); }
