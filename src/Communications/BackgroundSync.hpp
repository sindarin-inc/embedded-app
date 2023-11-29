#include "Misc/PowerManager.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"

void BackgroundSyncCancel();

void BackgroundSync(PowerManager &powerManager, PreferencesStore &prefs, LocalStore &localStore,
                    Telemetry &telemetry);

void BackgroundSyncAndSleep(PowerManager &powerManager, PreferencesStore &prefs,
                            LocalStore &localStore, Telemetry &telemetry);
