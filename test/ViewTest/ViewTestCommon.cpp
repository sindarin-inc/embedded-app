#include "ViewTestCommon.hpp"

#include <catch_amalgamated.hpp>
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
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

extern void UIInnerLoop(RemoteCommand command);
extern "C" void app_main(void); // NOLINT(readability-identifier-naming)
std::shared_ptr<std::thread> SchedulerThread;
const char *kGitHash = "SIMULATOR";

std::string pathPrefix = "./test/ViewTest/Images/";
std::string testRunPrefix = pathPrefix + "TestRun/";
std::string acceptedPathPrefix = pathPrefix + "Accepted/";

void appMainTask(void *parameters) {
    app_main();
    vTaskDelete(NULL);
}

void StartFreeRtos(bool runAppMain) {
    // The FreeRTOS tick handler happens via SIGALRM, and it also kicks off preemptive task
    // switching, which is not what we want on the main thread, since it's not a real FreeRTOS task.
    // Just disable SIGALRM on this thread so that it only happens on FreeRTOS tasks.
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    // Skip waveform processing. Technically not a FreeRTOS thing but this is a convenient spot to
    // do it
    FakeEinkET013TT1SkipWaveform = true;

    // Start the FreeRTOS scheduler
    SchedulerThread = std::make_unique<std::thread>([runAppMain]() {
        if (runAppMain) {
            // ESP-IDF does `app_main` in a task as well. We need to do this so we can use
            // FreeRTOS-timed stuff like vTaskDelay
            xTaskCreate(appMainTask, "app_main", 10000, NULL, 1, NULL);
        }
        vTaskStartScheduler();
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

void SendRemoteCommand(RemoteCommand command) {
    DispatchGetUiQueue()->dispatchFromISR([command]() { UIInnerLoop(command); });
}

// TODO: How do we use the test name also for the key
auto AssertViewUnchanged(std::string key) -> ViewTestResult {
    ViewTestResult result = {
        .passed = false,
        .acceptedImagePath = acceptedPathPrefix + key + ".png",
        .renderedImagePath = testRunPrefix + key + ".png",
    };

    // Render out the FakeEinkET013TT1ParticlePositions to a png
    stbi_write_png(result.renderedImagePath.c_str(), EinkET013TT1_PIXEL_WIDTH,
                   EinkET013TT1_PIXEL_HEIGHT, 1, FakeEinkET013TT1ParticlePositions, 0);

    // Load up the known-good png from the key
    int width, height, channels;
    unsigned char *data =
        stbi_load(result.acceptedImagePath.c_str(), &width, &height, &channels, 0);

    // Check if the image was successfully loaded
    if (data == nullptr) {
        UNSCOPED_INFO("Failed to load accepted image.");
        UNSCOPED_INFO("This is expected when testing the first time.");
        UNSCOPED_INFO("Run the following command to create an accepted test image:");
        UNSCOPED_INFO("  cp " << result.renderedImagePath << " \\\n  " << result.acceptedImagePath);
        return result;
    };

    // Bonus: Render a pixel-by-pixel diff image, save to png
    uint8_t diffData[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT];

    // Compare pixel by pixel the values to make sure they all match
    result.passed = true;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            int index = (y * width) + x;
            diffData[index] = abs(data[index] - FakeEinkET013TT1ParticlePositions[index]);
            if (data[index] != FakeEinkET013TT1ParticlePositions[index]) {
                result.passed = false;
            }
        }
    }

    if (!result.passed) {
        result.diffImagePath = testRunPrefix + key + "_diff.png";
        stbi_write_png(result.diffImagePath.c_str(), EinkET013TT1_PIXEL_WIDTH,
                       EinkET013TT1_PIXEL_HEIGHT, 1, diffData, 0);
        UNSCOPED_INFO("Images do not match:");
        UNSCOPED_INFO("  Accepted (old): " << result.acceptedImagePath.c_str());
        UNSCOPED_INFO("  Rendered (new): " << result.renderedImagePath.c_str());
        UNSCOPED_INFO("  Diff: " << result.diffImagePath.c_str());
        UNSCOPED_INFO("  Quickly open all three with command: ");
        UNSCOPED_INFO("    open " << result.acceptedImagePath.c_str() << " \\\n    "
                                  << result.renderedImagePath.c_str() << " \\\n    "
                                  << result.diffImagePath.c_str());
        UNSCOPED_INFO("  Accept this change: ");
        UNSCOPED_INFO("    cp " << result.renderedImagePath.c_str() << " \\\n    "
                                << result.acceptedImagePath.c_str());
    }

    stbi_image_free(data);
    CHECK(result.passed);
    return result;
}
