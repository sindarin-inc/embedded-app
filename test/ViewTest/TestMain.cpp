#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iostream>
#include <memory>
#include <signal.h>
#include <thread>
#include <unistd.h>

#include "Displays/FakeEinkET013TT1.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Math.hpp"
#include "Sensors/LightingController.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "sindarin-debug.h"

extern "C" void app_main(void); // NOLINT(readability-identifier-naming)
std::shared_ptr<std::thread> SchedulerThread;
const char *kGitHash = "SIMULATOR";

void appMainTask(void *parameters) {
    app_main();
    vTaskDelete(NULL);
}

void StartFreeRtos() {
    // The FreeRTOS tick handler happens via SIGALRM, and it also kicks off preemptive task
    // switching, which is not what we want on the main thread, since it's not a real FreeRTOS task.
    // Just disable SIGALRM on this thread so that it only happens on FreeRTOS tasks.
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    // Start the FreeRTOS scheduler
    SchedulerThread = std::make_unique<std::thread>([]() {
        // ESP-IDF does `app_main` in a task as well. We need to do this so we can use
        // FreeRTOS-timed stuff like vTaskDelay
        xTaskCreate(appMainTask, "app_main", 10000, NULL, 1, NULL);
        vTaskStartScheduler();
        log_i("Scheduler thread done");
    });
}

void StopFreeRtos() {
    vTaskEndScheduler();
    (*SchedulerThread).join();
}

// TODO: Add timeout
void WaitForDraw() {
    uint8_t out;
    while (FakeEinkET013TT1RenderQueue == nullptr ||
           xQueueReceiveFromISR(FakeEinkET013TT1RenderQueue, &out, nullptr) == pdFALSE) {
        usleep(1000);
    }
}

struct ViewTestResult {
    bool passed;
    std::string acceptedImagePath;
    std::string renderedImagePath;
    std::string diffImagePath;
};

auto AssertViewUnchanged(std::string key) -> ViewTestResult {
    // Render out the FakeEinkET013TT1ParticlePositions to a png
    // Save to a tmp file
    // Load up the png from the key
    // Compare pixel by pixel the values to make sure they all match
    // Bonus: Render a pixel-by-pixel diff image, save to png
    return {};
}

// TODO: Maybe use Catch.hpp to run individual tests
auto main(int argc, char *argv[]) -> int {
    StartFreeRtos();

    // while (mainLoop()) {
    //     // Limit to ~60fps so we don't murder battery unnecessarily
    //     usleep(1000000.0 / 60.0);
    // }

    FakeEinkET013TT1SkipWaveform = true;

    // First screen renders twice once with a full refresh and once with a AU4
    WaitForDraw();
    std::cerr << "Got one draw" << std::endl;
    WaitForDraw();
    std::cerr << "Got one draw" << std::endl;

    AssertViewUnchanged("FirstScreen");

    // Save to an image

    // Compare with expected image

    log_i("Shutting down");

    StopFreeRtos();
    log_i("Tests done");

    return 0;
}
