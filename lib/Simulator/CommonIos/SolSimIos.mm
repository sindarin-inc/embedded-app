#include "SolSimIos.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"
#include "Misc/Dispatch.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Math.hpp"

// Forward declarations
extern void UIInnerLoop(RemoteCommand command);
extern "C" void app_main(void);
extern int LightingSetting;
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

void StartFreeRTOSScheduler() {
    // Create the app_main task
    xTaskCreate(AppMainTask, "app_main", 10000, NULL, 1, NULL);
    // Start the scheduler
    vTaskStartScheduler();
}

void StopFreeRTOSScheduler() {
    vTaskEndScheduler();
}

void HandleButtonPress(SolRemoteButton button) {
    RemoteCommand command;
    switch (button) {
        case SolRemoteButton::North:
            command = RemoteCommand::UP;
            break;
        case SolRemoteButton::South:
            command = RemoteCommand::DOWN;
            break;
        case SolRemoteButton::East:
            command = RemoteCommand::RIGHT;
            break;
        case SolRemoteButton::West:
            command = RemoteCommand::LEFT;
            break;
        case SolRemoteButton::Enter:
            command = RemoteCommand::ENTER;
            break;
    }
    DispatchGetUiQueue()->dispatchFromISR([command]() { UIInnerLoop(command); });
}

int ScreenWidth() {
    return myDisplay.nativeWidth();
}

int ScreenHeight() {
    return myDisplay.nativeHeight();
}

void SetSkipWaveform(bool skip) {
#if CONFIG_DISPLAY_EINKET013TT1
    myDisplay.setSkipWaveform(skip);
#endif
}

uint8_t *GetDisplayBuffer() {
    return myDisplay.buffer().get();
}

void SetLightingSetting(int lightingSetting) {
    LightingSetting = lightingSetting;
}

int GetLightingSetting() {
    return LightingSetting;
}

int GetLightingMinDutyCycle() {
    return GetMinLightingDutyCycle();
}

int GetLightingMaxDutyCycle() {
    return GetMaxLightingDutyCycle();
}

bool NeedsRedraw() {
    bool out;
    return myDisplay.renderQueue() != NULL &&
        xQueueReceiveFromISR(myDisplay.renderQueue(), &out, NULL) == pdTRUE;
}
