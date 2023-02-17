#include "Storage/PreferencesStore.hpp"

class PowerManager {
public:
    static auto getInstance() -> PowerManager &;

    PowerManager(PreferencesStore &prefs) : prefs_(prefs) {}

    void PowerOff();

protected:
    PreferencesStore &prefs_;
};

/*

#if !SIMULATOR
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Each task (Radio, Root UI, etc.) should register with the PowerManager to communicate what power
// level is needed for the function by acquiring a semaphore. The PowerManager will then be
// responsible for managing the power state of the device.
//
// Device funcitons:
//
// - Radio
//   -- BLE Advertising
//   -- Wifi Advertising
// - Content Sync (Max CPU)
// - Root UI (Min CPU)
// - Screen Refresh (MAX CPU)
// - Reading a page (Light sleep)
// - Close temples (Deep sleep)
// - Open temples (Light sleep)
// - Plug USB in (Max CPU, then Min CPU)
//

enum PowerState {
    // All power is off in this state - no power to the board, shouldn't be running code if this is
    // the case (obviously)
    PowerStatusMainPower = 1 << 0;
    // USB is connected
    PowerStatusUSB = 1 << 1;
    // Temples are open
    PowerStatusTemples = 1 << 2;
    // Battery is charging
    PowerStatusCharging = 1 << 3;
    // Battery is low
    PowerStatusLowBattery = 1 << 4;
    // Deep sleeping
    PowerStatusDeepSleep = 1 << 5;
    // Light sleeping (like when reading a page)
    PowerStatusLightSleep = 1 << 6;
    // Min CPU
    PowerStatusMinCPU = 1 << 7;
    // Max CPU
    PowerStatusMaxCPU = 1 << 8;
};

class PowerManager {
public:
    PowerManager() : states_(0) {}

    void PowerOn() {
        // Start registered power on tasks
        for (auto &task : power_on_tasks_) {
            xTaskCreate(task, "PowerOnTask", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY,
                        NULL);
        }
    }

    void PowerOff() {
        // Perform any necessary cleanup for power off
        blue_led_.Off();

        // Start registered power off tasks
        for (auto &task : power_off_tasks_) {
            xTaskCreate(task, "PowerOffTask", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY,
                        NULL);
        }

        // Enter deep sleep or other low-power mode
        // ...
    }

    void RegisterPowerOnTask(TaskFunction_t task) { power_on_tasks_.push_back(task); }

    void RegisterPowerOffTask(TaskFunction_t task) { power_off_tasks_.push_back(task); }

private:
    PowerState states_;

    // Insert any necessary member variables here
    std::vector<TaskFunction_t> power_on_tasks_;
    std::vector<TaskFunction_t> power_off_tasks_;
};

#endif

*/