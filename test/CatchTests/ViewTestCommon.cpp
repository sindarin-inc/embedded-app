#include "ViewTestCommon.hpp"

#include <catch_amalgamated.hpp>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iostream>
#include <memory>
#include <semaphore>
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
#include "Misc/Defer.hpp"
#include "Misc/Dispatch.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "sindarin-debug.h"

// TESTS_ACCEPT_CHANGES=1|true|yes|on (default: off)
static auto TestsAcceptChanges() -> bool {
    static int cached = -1;
    if (cached == -1) {
        bool enabled = false;
        const char *env = std::getenv("TESTS_ACCEPT_CHANGES");
        if (env != nullptr) {
            std::string value(env);
            for (char &ch : value) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            enabled = (value == "1" || value == "true" || value == "yes" || value == "on");
        }
        cached = enabled ? 1 : 0;
    }
    return cached == 1;
}

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

void MockClock() {
    useSimulatorTestTime = true;
    simulatorTestHours = 12;
    simulatorTestMinutes = 21;
}

void UnMockClock() { useSimulatorTestTime = false; }

void appMainTask(void *parameters) {
    MockClock();
    app_main();
    vTaskDelete(NULL);
}

pthread_t mainThread;

void StartFreeRtos(bool runAppMain) {
    static bool started = false;
    if (started) {
        assert(false && "FreeRTOS scheduler already started");
        return;
    }
    started = true;

    std::binary_semaphore semaphore(0);

    // Start the FreeRTOS scheduler
    assert(SchedulerThread == nullptr && "FreeRTOS scheduler already started");
    SchedulerThread = std::make_shared<std::thread>([runAppMain, &semaphore]() {
        if (runAppMain) {
            // ESP-IDF does `app_main` in a task as well. We need to do this so we can use
            // FreeRTOS-timed stuff like vTaskDelay
            xTaskCreate(appMainTask, "app_main", 10000, NULL, 1, NULL);
        }
        // Task to signal that the scheduler has started
        xTaskCreate(
            [](void *param) {
            log_i("FreeRTOS scheduler started");
            auto s = static_cast<std::binary_semaphore *>(param);
            s->release(); // Signal that the worker has completed
            vTaskDelete(nullptr);
            },
            "start", 10000, &semaphore, 1, nullptr);
        log_i("Starting FreeRTOS scheduler");
        vTaskStartScheduler();
        log_i("FreeRTOS scheduler exited");
    });

    // Wait until the scheduler has started
    semaphore.try_acquire_for(std::chrono::seconds(5));
}

void StartAppMain(FONT_CHOICES font, int x, int y, int lineSpacing, std::string prefsDir) {
    // Skip waveform processing. Technically not a FreeRTOS thing but this is a convenient spot to
    // do it
    GetRootDisplay().setSkipWaveform(true);

    char prefsFile[PATH_MAX];
    snprintf(prefsFile, sizeof(prefsFile), "%s/simprefs.ini", prefsDir.c_str());
    // Delete the prefs file and start fresh
    std::filesystem::remove(prefsFile);
    prefs.begin(prefsFile);
    prefs.invertSetting.set(0);
    prefs.font.set(font);
    prefs.lineSpacing.set(lineSpacing);
    prefs.dontShowWelcome.set(true);
    prefs.textInsetLeft.set(x);
    prefs.textInsetRight.set(x);
    prefs.textInsetTop.set(y);
    prefs.textInsetBottom.set(y);
    prefs.bookSortCriteria.set(0);
    prefs.bookSortDirection.set(0);
    prefs.firstRead.set(0);
    prefs.lastReadableId.remove();
    prefs.readablePlace.remove("93d56842");

    StartFreeRtos(true);

    // Wait for screen refresh at boot (from screenClearedAtShutdown)
    WAIT_FOR_DRAW(GetRootDisplay());
}

void StopFreeRtos() {
    log_i("Stopping FreeRTOS scheduler");
    DispatchBeginShutdown();
    // Wait a couple seconds for the dispatch queues to finish
    usleep(2000000);
    vTaskEndScheduler();
    (*SchedulerThread).join();
    SchedulerThread = nullptr;
    log_i("Stopped FreeRTOS scheduler");
}

auto WaitForDraw(DisplayEinkET013TT1 &display, bool continueAfterDraw, long timeoutSeconds)
    -> bool {
    struct timespec startTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    long elapsedTime = 0;
    uint8_t out;
    useconds_t sleep = 1000;
    while (display.renderQueue() == nullptr ||
           xQueuePeekFromISR(display.renderQueue(), &out) == pdFALSE) {
        if (elapsedTime > timeoutSeconds) {
            UNSCOPED_INFO("Timed out waiting for draw");
            return false;
        }
        usleep(sleep);
        sleep = std::min(sleep * 2, (useconds_t)200000);
        struct timespec endTime;
        clock_gettime(CLOCK_MONOTONIC, &endTime); // get current time
        elapsedTime = endTime.tv_sec - startTime.tv_sec;
    }
    if (continueAfterDraw) {
        auto result = xQueueReceiveFromISR(display.renderQueue(), &out, nullptr);
        if (result == pdFALSE) {
            log_w("Warning: Failed to empty render queue after waiting");
        }
    }

    return true;
}

auto HasQueuedDraw(DisplayEinkET013TT1 &display) -> bool {
    uint8_t out;
    return display.renderQueue() != nullptr &&
           xQueuePeekFromISR(display.renderQueue(), &out) == pdTRUE;
}

void SendRemoteCommand(RemoteCommand command) {
    if (HasQueuedDraw(GetRootDisplay())) {
        log_w("Warning: Remote command %s sent while there is a queued draw",
              CodeToString(command));
    }
    log_i("Dispatching remote command %s", CodeToString(command));
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

// Convert back to a repo-relative path for logging only.
static auto RepoRelative(const std::string &absolutePath) -> std::string {
    return "test/CatchTests/" + absolutePath.substr(GetDirectory().size());
}

// TODO: How do we use the test name also for the key
auto CheckViewUnchanged(DisplayEinkET013TT1 &display, std::string key) -> ViewTestResult {
    log_i("Checking view unchanged for %s", key.c_str());
    auto directory = GetDirectory();
    ViewTestResult result = {
        .passed = false,
        .acceptedImagePath = directory + acceptedPathPrefix + key + ".png",
        .renderedImagePath = directory + testRunPrefix + key + ".png",
    };

    // Create the TestRun directory and any parent directories if needed
    // This handles both the base TestRun directory and any subdirectories in the key
    std::filesystem::create_directories(
        std::filesystem::path(result.renderedImagePath).parent_path());

    defer {
        // Before returning, allow the display to continue
        uint8_t out;
        xQueueReceiveFromISR(display.renderQueue(), &out, nullptr);
    };

    log_i("Saving image to %s (width=%d, height=%d)",
          RepoRelative(result.renderedImagePath).c_str(), CONFIG_DISPLAY_WIDTH,
          CONFIG_DISPLAY_HEIGHT);

    if (!stbi_write_png(result.renderedImagePath.c_str(), CONFIG_DISPLAY_WIDTH,
                        CONFIG_DISPLAY_HEIGHT, 1, display.buffer().get(), 0)) {
        log_e("Unable to save rendered image to %s",
              RepoRelative(result.renderedImagePath).c_str());
    }

    // Load up the known-good png from the key
    int width, height, channels;
    unsigned char *data =
        stbi_load(result.acceptedImagePath.c_str(), &width, &height, &channels, 0);

    // Check if the image was successfully loaded
    if (data == nullptr) {
        UNSCOPED_INFO("Failed to find test image.");
        UNSCOPED_INFO("  Missing : " << RepoRelative(result.acceptedImagePath));
        UNSCOPED_INFO("  Rendered: " << RepoRelative(result.renderedImagePath));

        if (TestsAcceptChanges()) {
            WARN("Saving new test image (TESTS_ACCEPT_CHANGES=1)\n  New: "
                 << RepoRelative(result.acceptedImagePath));
            // Create the accepted directory if it doesn't exist
            std::filesystem::create_directories(
                std::filesystem::path(result.acceptedImagePath).parent_path());
            std::filesystem::copy(result.renderedImagePath, result.acceptedImagePath,
                                  std::filesystem::copy_options::overwrite_existing);
            result.passed = true;
        }
        return result;
    };

    // Bonus: Render a pixel-by-pixel diff image, save to png
    uint8_t diffData[CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT];

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
        if (!stbi_write_png(result.diffImagePath.c_str(), CONFIG_DISPLAY_WIDTH,
                            CONFIG_DISPLAY_HEIGHT, 1, diffData, 0)) {
            log_e("Unable to save diff image to %s", RepoRelative(result.diffImagePath).c_str());
        }

        UNSCOPED_INFO("Images do not match:");
        UNSCOPED_INFO("  Accepted: " << RepoRelative(result.acceptedImagePath));
        UNSCOPED_INFO("  Rendered: " << RepoRelative(result.renderedImagePath));
        UNSCOPED_INFO("  Diff:     " << RepoRelative(result.diffImagePath));
        if (TestsAcceptChanges()) {
            WARN("Updating test image (TESTS_ACCEPT_CHANGES=1)\n  Updated: "
                 << RepoRelative(result.acceptedImagePath));
            // Create the accepted directory if it doesn't exist
            std::filesystem::create_directories(
                std::filesystem::path(result.acceptedImagePath).parent_path());
            std::filesystem::copy(result.renderedImagePath, result.acceptedImagePath,
                                  std::filesystem::copy_options::overwrite_existing);
            result.passed = true;
        }
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
