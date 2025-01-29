#include "SolSimIos.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"
#include "Misc/Dispatch.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Math.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

// Forward declarations
extern void UIInnerLoop(RemoteCommand command);
extern "C" void app_main(void);
extern int LightingSetting;
extern PosixFileSystem fileSystem;
extern PreferencesStore prefs;
#define protected public
#if CONFIG_DISPLAY_SIM_8BIT
#include "Displays/DisplaySim8Bit.hpp"
extern DisplaySim8Bit myDisplay;
#elif CONFIG_DISPLAY_SIM_1BIT
#include "Displays/DisplaySim1Bit.hpp"
extern DisplaySim1Bit myDisplay;
#elif CONFIG_DISPLAY_EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
extern DisplayEinkET013TT1 myDisplay;
#else
#error "No display selected"
#endif
#undef protected

static void AppMainTask(void *pvParameters) {
    app_main();
    vTaskDelete(NULL);
}

extern "C" void StartFreeRTOSScheduler() {
    // Create the app_main task
    xTaskCreate(AppMainTask, "app_main", 10000, NULL, 1, NULL);
    // Start the scheduler
    vTaskStartScheduler();
}

extern "C" void StopFreeRTOSScheduler() {
    vTaskEndScheduler();
}

extern "C" void HandleButtonPress(SolRemoteButton button) {
    RemoteCommand command;
    switch (button) {
        case SolRemoteButtonNorth:
            command = RemoteCommand::UP;
            break;
        case SolRemoteButtonSouth:
            command = RemoteCommand::DOWN;
            break;
        case SolRemoteButtonEast:
            command = RemoteCommand::RIGHT;
            break;
        case SolRemoteButtonWest:
            command = RemoteCommand::LEFT;
            break;
        case SolRemoteButtonEnter:
            command = RemoteCommand::ENTER;
            break;
    }
    DispatchGetUiQueue()->dispatchFromISR([command]() { UIInnerLoop(command); });
}

extern "C" int ScreenWidth() {
    return myDisplay.nativeWidth();
}

extern "C" int ScreenHeight() {
    return myDisplay.nativeHeight();
}

extern "C" void SetSkipWaveform(bool skip) {
#if CONFIG_DISPLAY_EINKET013TT1
    myDisplay.setSkipWaveform(skip);
#endif
}

extern "C" uint8_t *GetDisplayBuffer() {
    return myDisplay.buffer().get();
}

extern "C" void SetLightingSetting(int lightingSetting) {
    LightingSetting = lightingSetting;
}

extern "C" int GetLightingSetting() {
    return LightingSetting;
}

extern "C" int GetLightingMinDutyCycle() {
    return GetMinLightingDutyCycle();
}

extern "C" int GetLightingMaxDutyCycle() {
    return GetMaxLightingDutyCycle();
}

extern "C" bool NeedsRedraw() {
    bool out;
    return myDisplay.renderQueue() != NULL &&
        xQueueReceiveFromISR(myDisplay.renderQueue(), &out, NULL) == pdTRUE;
}

extern "C" void SetFileSystemPrefix(const char *prefix) {
    fileSystem.setPrefix(prefix);
}

extern "C" void SetupPrefs(const char *path) {
    prefs.begin(path);
}

extern "C" void SetCloudToken(const char *token) {
    prefs.cloudToken.set(token);
}
