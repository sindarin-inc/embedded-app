#include "ViewTestCommon.hpp"

#include <catch_amalgamated.hpp>
#include <cstring>
#include <filesystem>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iostream>
#include <memory>
#include <signal.h>
#include <thread>
#include <unistd.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#pragma clang diagnostic pop

#include "Displays/FakeEinkET013TT1.hpp"
#include "Misc/Dispatch.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "sindarin-debug.h"

#define ACCEPT_CHANGES 1

extern bool useSimulatorTestTime;
extern int simulatorTestHours;
extern int simulatorTestMinutes;

// Created in main.cpp
extern PreferencesStore prefs;
extern void UIInnerLoop(RemoteCommand command);
extern "C" void app_main(void); // NOLINT(readability-identifier-naming)
std::shared_ptr<std::thread> SchedulerThread;
extern PosixFileSystem fileSystem;
extern DisplayEinkET013TT1 myDisplay;

std::string directory;
std::string testRunPrefix = "Images/TestRun/";
std::string acceptedPathPrefix = "Images/Accepted/";

void appMainTask(void *parameters) {
    useSimulatorTestTime = true;
    simulatorTestHours = 12;
    simulatorTestMinutes = 21;
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

    // Start the FreeRTOS scheduler
    assert(SchedulerThread == nullptr && "FreeRTOS scheduler already started");
    SchedulerThread = std::make_shared<std::thread>([runAppMain]() {
        if (runAppMain) {
            // ESP-IDF does `app_main` in a task as well. We need to do this so we can use
            // FreeRTOS-timed stuff like vTaskDelay
            xTaskCreate(appMainTask, "app_main", 10000, NULL, 1, NULL);
        }
        log_i("Starting FreeRTOS scheduler");
        vTaskStartScheduler();
        log_i("FreeRTOS scheduler exited");
    });
}

void StartAppMain(FONT_CHOICES font, int x, int y, int lineSpacing) {
    // Skip waveform processing. Technically not a FreeRTOS thing but this is a convenient spot to
    // do it
    GetRootDisplay().setSkipWaveform(true);

    auto prefsFile = "test/CatchTests/TestData/simprefs.ini";
    // Delete the prefs file and start fresh
    std::filesystem::remove(prefsFile);
    prefs.begin(prefsFile);
    prefs.invertSetting.set(0);
    prefs.font.set(font);
    prefs.lineSpacing.set(lineSpacing);
    prefs.rectX.set(x);
    prefs.rectY.set(y);
    prefs.rectW.set(256 - x * 2);
    prefs.rectH.set(256 - y * 2);
    prefs.bookSortCriteria.set(0);
    prefs.bookSortDirection.set(0);
    prefs.firstRead.set(0);
    prefs.pageTransitionArrows.set(false);
    prefs.lastReadableId.remove();
    prefs.readablePlace.remove("93d56842");

    StartFreeRtos(true);

    // Wait for screen refresh at boot (from screenClearedAtShutdown)
    WAIT_FOR_DRAW(GetRootDisplay());
}

void StopFreeRtos() {
    log_i("Stopping FreeRTOS scheduler");
    vTaskEndScheduler();
    (*SchedulerThread).join();
    SchedulerThread = nullptr;
    log_i("Stopped FreeRTOS scheduler");
}

auto WaitForDraw(DisplayEinkET013TT1 &display, long timeoutSeconds) -> bool {
    struct timespec startTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    long elapsedTime = 0;
    uint8_t out;
    useconds_t sleep = 1000;
    while (display.renderQueue() == nullptr ||
           xQueueReceiveFromISR(display.renderQueue(), &out, nullptr) == pdFALSE) {
        if (elapsedTime > timeoutSeconds) {
            UNSCOPED_INFO("Timed out waiting for draw " << display.renderQueue());
            return false;
        }
        usleep(sleep);
        sleep = std::min(sleep * 2, (useconds_t)200000);
        struct timespec endTime;
        clock_gettime(CLOCK_MONOTONIC, &endTime); // get current time
        elapsedTime = endTime.tv_sec - startTime.tv_sec;
    }
    return true;
}

void SendRemoteCommand(RemoteCommand command) {
    DispatchGetUiQueue()->dispatchFromISR([command]() { UIInnerLoop(command); });
}

void SetImagesFolder(std::string folder) {
    testRunPrefix = folder + "/TestRun/";
    acceptedPathPrefix = folder + "/Accepted/";
}

auto GetDirectory(std::string filename = "") -> std::string {
    // gets the current source file path
    std::string path;
    if (filename.empty()) {
        path = __FILE__;
    } else {
        path = filename;
    }

    // Extract the directory by finding the last '/' character and
    // removing everything after it
    size_t pos = path.find_last_of("/\\");
    std::string dir = path.substr(0, pos + 1);

    return dir;
}

// TODO: How do we use the test name also for the key
auto CheckViewUnchanged(DisplayEinkET013TT1 &display, std::string key) -> ViewTestResult {
    auto directory = GetDirectory();
    ViewTestResult result = {
        .passed = false,
        .acceptedImagePath = directory + acceptedPathPrefix + key + ".png",
        .renderedImagePath = directory + testRunPrefix + key + ".png",
    };

    // Create the TestRun directory if needed
    std::filesystem::create_directory(GetDirectory(directory + testRunPrefix));
    std::filesystem::create_directory(GetDirectory(directory + testRunPrefix + key));

    if (!stbi_write_png(result.renderedImagePath.c_str(), EinkET013TT1_PIXEL_WIDTH,
                        EinkET013TT1_PIXEL_HEIGHT, 1, display.buffer().get(), 0)) {
        log_e("Unable to save rendered image to %s", result.renderedImagePath.c_str());
    }

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
        CHECK(data != nullptr);
        // If ACCEPT_CHANGES is defined, automatically accept the changes
#ifdef ACCEPT_CHANGES
        UNSCOPED_INFO("  ---> Accepting changes");
        std::filesystem::copy(result.renderedImagePath, result.acceptedImagePath,
                              std::filesystem::copy_options::overwrite_existing);
#endif
        return result;
    };

    // Bonus: Render a pixel-by-pixel diff image, save to png
    uint8_t diffData[EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT];

    // Compare pixel by pixel the values to make sure they all match
    result.passed = true;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            int index = (y * width) + x;
            diffData[index] = abs(data[index] - display.buffer()[index]);
            if (data[index] != display.buffer()[index]) {
                result.passed = false;
            }
        }
    }

    if (!result.passed) {
        // Render out the diff image to a png
        result.diffImagePath = directory + testRunPrefix + key + "_diff.png";
        if (!stbi_write_png(result.diffImagePath.c_str(), EinkET013TT1_PIXEL_WIDTH,
                            EinkET013TT1_PIXEL_HEIGHT, 1, diffData, 0)) {
            log_e("Unable to save diff image to %s", result.diffImagePath.c_str());
        }

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
        // If ACCEPT_CHANGES is defined, automatically accept the changes
#ifdef ACCEPT_CHANGES
        UNSCOPED_INFO("  ---> Accepting changes");
        std::filesystem::copy(result.renderedImagePath, result.acceptedImagePath,
                              std::filesystem::copy_options::overwrite_existing);
#endif
    } else {
        // Delete generated image files for passing tests
        remove(result.renderedImagePath.c_str());
    }

    stbi_image_free(data);
    return result;
}

auto CheckViewUnchanged(std::shared_ptr<DisplayEinkET013TT1> display, std::string key)
    -> ViewTestResult {
    return CheckViewUnchanged(*display, key);
}

auto GetRootDisplay() -> DisplayEinkET013TT1 & { return myDisplay; }

void SetupFileSystemMock(const std::string &directory) {
    auto path = GetDirectory() + directory;
    log_i("Setting filesystem prefix to %s", path.c_str());
    fileSystem.setPrefix(path);

    // Clear bookmarks & logs directories
    auto bookmarks = path + std::string("/bookmarks");
    std::filesystem::remove_all(bookmarks);
    auto logs = path + std::string("/logs");
    std::filesystem::remove_all(logs);
}

auto LoadJsonDoc(FileSystem &fileSystem, const std::string &path)
    -> std::unique_ptr<SpiRamJsonDocument> {
    SFile readablesFile = fileSystem.open(path);
    if (!readablesFile || (int)readablesFile.size() == 0) {
        log_w("Failed to open %s", path.c_str());
        return nullptr;
    }

    // Grab a minimum amount of ram, even if the readables file is empty
    auto docSize = (int)readablesFile.size() * 2;
    auto doc = std::make_unique<SpiRamJsonDocument>(docSize);
    auto error = deserializeJson(*doc, readablesFile);
    readablesFile.close();

    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize readables file: %s", error.c_str());
        return nullptr;
    }

    return doc;
}
