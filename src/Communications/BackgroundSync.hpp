#include "Misc/PowerManager.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"

void BackgroundSync(PowerManager &powerManager, PreferencesStore &prefs, LocalStore &localStore);

void BackgroundSyncAndSleep(PowerManager &powerManager, PreferencesStore &prefs,
                            LocalStore &localStore);
