#include "Misc/PowerManager.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"

void BackgroundSyncCancel();

auto BackgroundSync(PowerManager &powerManager, PreferencesStore &prefs, LocalStore &localStore,
                    Telemetry &telemetry, FileSystem &fileSystem, bool useWifi, bool useBle)
    -> bool;

void __attribute__((noreturn))
BackgroundSyncAndSleep(PowerManager &powerManager, PreferencesStore &prefs, LocalStore &localStore,
                       Telemetry &telemetry, FileSystem &fileSystem);
