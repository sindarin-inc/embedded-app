#if defined(EMSCRIPTEN)
#include <emscripten.h>
#endif
#include <SDL2/SDL.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iostream>
#include <signal.h>
#include <sstream>
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
#include "Misc/Math.hpp"
#include "Misc/Platform.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

#if SIMULATOR_BLE
#include "ble.hpp"
#endif
#include "Inputs/RemoteInput.hpp"

#define SIM_LED_ON 1
#define SIM_LED_ROUND 1
#if SIM_LED_ON
#define SIM_LED_DIM 6
#else
#define SIM_LED_DIM 0
#endif

extern PreferencesStore prefs;
extern void UIInnerLoop(RemoteCommand command);
extern Screen *UIGetCurrentScreen();

extern "C" void app_main(void); // NOLINT(readability-identifier-naming)

const char *kGitHash = "SIMULATOR";

#define SCALE 2

// Will be set by PlatformSimulator.cpp SetLeftScreenLight() and SetRightScreenLight()
extern int LightingSetting;
extern bool simLeds[3];
extern bool ledChange;

SDL_Window *window;
SDL_Renderer *renderer;
int totalFrames = 0;
bool movieOn_ = false;

// Texture on which to draw the UI from the firmware
SDL_Texture *deviceScreen;

float lerp(float a, float b, float t) { return a + t * (b - a); }

bool saveScreenshotPng(std::string filepath, SDL_Window *window, SDL_Renderer *renderer) {
    if (!stbi_write_png(filepath.c_str(), EinkET013TT1_PIXEL_WIDTH, EinkET013TT1_PIXEL_HEIGHT, 1,
                        FakeEinkET013TT1ParticlePositions, 0)) {
        log_e("Unable to save rendered image to %s", filepath.c_str());
    }

    return true;
}

void SimDrawScreen() {
    for (int x = 0; x < EinkET013TT1_PIXEL_WIDTH; x++) {
        for (int y = 0; y < EinkET013TT1_PIXEL_HEIGHT; y++) {
            uint8_t color = FakeEinkET013TT1ParticlePositions[x + y * EinkET013TT1_PIXEL_WIDTH];
            float colorF = color / 255.0;
            uint16_t brightnessOffset =
                MapRange(LightingSetting, static_cast<int>(GetMinLightingDutyCycle()),
                         static_cast<int>(GetMaxLightingDutyCycle()), 60, 0);
            SDL_SetRenderDrawColor(
                renderer, lerp(40 + brightnessOffset, 248 - brightnessOffset, colorF),
                lerp(40 + brightnessOffset, 240 - brightnessOffset, colorF),
                lerp(40 + brightnessOffset, 227 - brightnessOffset, colorF), 0xFF);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }

    if (movieOn_) {
        totalFrames++;
        char filename[100];
        snprintf(filename, sizeof(filename), "frames/frame-%05d.png", totalFrames);
        saveScreenshotPng(filename, window, renderer);
    }
}

void DrawRectLed(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

    SDL_SetRenderDrawColor(renderer, r, g, b, 0xFF);
    SDL_RenderFillRect(renderer, &rect);
}

void DrawRoundLed(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    float centerX = (float)x + ((float)w / 2.0);
    float centerY = (float)y + ((float)h / 2.0);
    float rad = centerX - (float)x;

    for (int xx = x; xx < x + w; xx++) {
        for (int yy = y; yy < y + h; yy++) {
            float dx = (float)xx - centerX;
            float dy = (float)yy - centerY;
            float dist = sqrt(dx * dx + dy * dy);
            if (dist > rad) {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
            } else {
                float f = 1.0 - (dist / rad);
                uint8_t rr = (uint8_t)((float)r * f);
                uint8_t gg = (uint8_t)((float)g * f);
                uint8_t bb = (uint8_t)((float)b * f);
                SDL_SetRenderDrawColor(renderer, rr, gg, bb, 0xFF);
            }
            SDL_RenderDrawPoint(renderer, xx, yy);
        }
    }
}

void SimDrawLeds() {
#if SIM_LED_ON
    // draw simulated leds
    int ledDim = SIM_LED_DIM;
    int ledStartY = EinkET013TT1_PIXEL_HEIGHT;
    int ledStartX = (EinkET013TT1_PIXEL_WIDTH / 2) - (ledDim / 2);

    // clear area
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
    SDL_Rect rect;
    rect.x = 0;
    rect.y = ledStartY;
    rect.h = ledDim;
    rect.w = EinkET013TT1_PIXEL_WIDTH;

    SDL_RenderFillRect(renderer, &rect);

    uint8_t color[3];
    for (int i = 0; i < 3; i++) {
        bool ledVal = simLeds[i];
        memset(color, 0, 3);

        if (ledVal) {
            color[i] = 0xFF;
        }
#if SIM_LED_ROUND
        DrawRoundLed(ledStartX, ledStartY, ledDim, ledDim, color[0], color[1], color[2]);
#else
        DrawRectLed(ledStartX, ledStartY, ledDim, ledDim, color[0], color[1], color[2]);
#endif
        ledStartX += ledDim;
    }
    ledChange = false;
#endif
};

bool isPngEqual(std::string filepath1, std::string filepath2) {
    int width1, height1, bpp1;
    int width2, height2, bpp2;

    uint8_t *img1 = stbi_load(filepath1.c_str(), &width1, &height1, &bpp1, 3);
    uint8_t *img2 = stbi_load(filepath2.c_str(), &width2, &height2, &bpp2, 3);

    if (img1 == nullptr || img2 == nullptr) {
        std::cerr << "Failed to load one or both images\n";
        return false;
    }

    if (width1 != width2 || height1 != height2 || bpp1 != bpp2) {
        std::cerr << "Images are not the same size or color depth\n";
        stbi_image_free(img1);
        stbi_image_free(img2);
        return false;
    }

    for (int y = 0; y < height1; y++) {
        for (int x = 0; x < width1; x++) {
            int idx = (y * width1 + x) * 3;
            if (img1[idx] != img2[idx] || img1[idx + 1] != img2[idx + 1] ||
                img1[idx + 2] != img2[idx + 2]) {
                std::cerr << "Pixel difference at " << x << ", " << y << "\n";
                std::cerr << "Image 1: " << (int)img1[idx] << " " << (int)img1[idx + 1] << " "
                          << (int)img1[idx + 2] << "\n";
                std::cerr << "Image 2: " << (int)img2[idx] << " " << (int)img2[idx + 1] << " "
                          << (int)img2[idx + 2] << "\n";
                stbi_image_free(img1);
                stbi_image_free(img2);
                return false;
            }
        }
    }

    stbi_image_free(img1);
    stbi_image_free(img2);

    return true;
}

#if SIMULATOR_BLE
// BLE command from the TurnTouch remote
RemoteCommand ttCommand;
#endif

void sendCodeAndRender(RemoteCommand command, SDL_Renderer *renderer) {
    DispatchGetUiQueue()->dispatchFromISR([command]() { UIInnerLoop(command); });

    // Wait for the UI thread to do the rendering to be rendered
    uint8_t out;
    while (xQueueReceiveFromISR(FakeEinkET013TT1RenderQueue, &out, nullptr) == pdFALSE) {
        usleep(10000);
    }

    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, deviceScreen, NULL, NULL);
    SimDrawScreen();
    SimDrawLeds();
    SDL_RenderPresent(renderer);
}

// Only press 'u' when you are on a page in a readable.
void runBackwardsUnitTest(SDL_Window *window, SDL_Renderer *renderer) {
    auto skipWaveform = FakeEinkET013TT1SkipWaveform;
    FakeEinkET013TT1SkipWaveform = true;

    auto screen = UIGetCurrentScreen();
    auto readableScreen = dynamic_cast<ReadableScreen *>(screen);

    if (readableScreen != nullptr) {
        bool first = true;
        for (int i = 0;; i++) {
            if (!first) {
                saveScreenshotPng("current.png", window, renderer);
                if (readableScreen->checkAtEnd()) break;
                sendCodeAndRender(RemoteCommand::ENTER, renderer);
            }
            first = false;
            saveScreenshotPng("next.png", window, renderer);
            if (readableScreen->checkAtEnd()) break;
            sendCodeAndRender(RemoteCommand::ENTER, renderer);
            saveScreenshotPng("nextnext.png", window, renderer);
            sendCodeAndRender(RemoteCommand::LEFT, renderer);
            saveScreenshotPng("nextprev.png", window, renderer);
            // compare the most two recent pngs -- they should be the same.
            // if not, error out.
            if (!isPngEqual("./next.png", "./nextprev.png")) {
                std::cerr << "images don't match, sad face\n";
                std::cerr << "check out next.png and nextprev.png for the diffs\n";
                exit(1);
            }
        }
        std::cerr << "Unit test passed\n";
    } else {
        std::cerr << "Not in a readableScreen!!\n";
    }
    FakeEinkET013TT1SkipWaveform = skipWaveform;
}

// This is used to save all png images of a book in support of the
// automated testing capability
void saveBookImages(SDL_Window *window, SDL_Renderer *renderer) {
    auto skipWaveform = FakeEinkET013TT1SkipWaveform;
    FakeEinkET013TT1SkipWaveform = true;

    auto screen = UIGetCurrentScreen();
    auto readableScreen = dynamic_cast<ReadableScreen *>(screen);

    if (readableScreen != nullptr) {
        for (int i = 0;; i++) {
            std::ostringstream fname;
            fname << "bookImages/" << i << ".png";
            saveScreenshotPng(fname.str(), window, renderer);
            if (readableScreen->checkAtEnd()) break;
            sendCodeAndRender(RemoteCommand::ENTER, renderer);
        }
        std::cerr << "Save Book Image Completed!" << std::endl;
    } else {
        std::cerr << "Not in a ReadableScreen!" << std::endl;
    }

    FakeEinkET013TT1SkipWaveform = skipWaveform;
}

// Only press 'a' when you are on a page in a readable.
void previousPageFast(SDL_Window *window, SDL_Renderer *renderer) {
    auto skipWaveform = FakeEinkET013TT1SkipWaveform;
    FakeEinkET013TT1SkipWaveform = true;

    sendCodeAndRender(RemoteCommand::LEFT, renderer);

    FakeEinkET013TT1SkipWaveform = skipWaveform;
    std::cerr << "unit test passed\n";
}

// Only press 's' when you are on a page in a readable.
void nextPageFast(SDL_Window *window, SDL_Renderer *renderer) {
    auto skipWaveform = FakeEinkET013TT1SkipWaveform;
    FakeEinkET013TT1SkipWaveform = true;

    sendCodeAndRender(RemoteCommand::ENTER, renderer);

    FakeEinkET013TT1SkipWaveform = skipWaveform;
    std::cerr << "unit test passed\n";
}

void goBackNumPages(SDL_Window *window, SDL_Renderer *renderer, int numPages) {
    int i = 0;

    for (i = 0; i < numPages; i++) {
        sendCodeAndRender(RemoteCommand::LEFT, renderer);
        usleep(1000000.0 / 240.0);
    }
}

void goBackwardNumPages(SDL_Window *window, SDL_Renderer *renderer, int numPages) {
    auto skipWaveform = FakeEinkET013TT1SkipWaveform;
    FakeEinkET013TT1SkipWaveform = true;

    while (numPages--) {
        sendCodeAndRender(RemoteCommand::LEFT, renderer);
        usleep(1000000.0 / 1.5);
    }
    FakeEinkET013TT1SkipWaveform = false;
}

void goForwardNumPages(SDL_Window *window, SDL_Renderer *renderer, int numPages) {
    auto skipWaveform = FakeEinkET013TT1SkipWaveform;
    FakeEinkET013TT1SkipWaveform = true;

    while (numPages--) {
        sendCodeAndRender(RemoteCommand::RIGHT, renderer);
        usleep(1000000.0 / 1.5);
    }
    FakeEinkET013TT1SkipWaveform = false;
}

// Returns false if the program should quit
auto mainLoop() -> bool {
    SDL_Event e;
    RemoteCommand remoteCode = RemoteCommand::NONE;
    while (SDL_PollEvent(&e)) {
        SDL_Keymod modState = SDL_GetModState();
        if (e.type == SDL_QUIT) {
            return false;
        } else if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
            case SDLK_UP:
                remoteCode = RemoteCommand::UP;
                break;
            case SDLK_DOWN:
                remoteCode = RemoteCommand::DOWN;
                break;
            case SDLK_LEFT:
                remoteCode = RemoteCommand::LEFT;
                break;
            case SDLK_RIGHT:
                remoteCode = RemoteCommand::RIGHT;
                break;
            case SDLK_ESCAPE:
                remoteCode = RemoteCommand::BACK;
                break;
            case SDLK_RETURN:
                remoteCode = RemoteCommand::ENTER;
                break;
            case 'p':
                saveScreenshotPng("./screenshot.png", window, renderer);
                break;
            case 'm':
                movieOn_ = !movieOn_;
                break;
            case 'u':
                runBackwardsUnitTest(window, renderer);
                break;
            case 'b':
                goBackNumPages(window, renderer, 50);
                break;
            case 'a':
                previousPageFast(window, renderer);
                break;
            case 's':
                nextPageFast(window, renderer);
                break;
            case 'q':
                // Ignore cmd-q (quit on macOS)
                if (!(modState & KMOD_GUI)) {
                    goBackwardNumPages(window, renderer, 200);
                }
                break;
            case 'w':
                // Ignore cmd-w (close on macOS)
                if (!(modState & KMOD_GUI)) {
                    goForwardNumPages(window, renderer, 200);
                }
                break;
            case 'i':
                saveBookImages(window, renderer);
                break;
            }

            if (remoteCode != RemoteCommand::NONE) {
                DispatchGetUiQueue()->dispatchFromISR([remoteCode]() { UIInnerLoop(remoteCode); });
            }
        }
    }

#if SIMULATOR_BLE
    if (ttCommand != RemoteCommand::NONE) {
        remoteCode = ttCommand;
        DispatchGetUiQueue()->dispatchFromISR([remoteCode]() { UIInnerLoop(remoteCode); });
        ttCommand = RemoteCommand::NONE;
    }
#endif

    // Check if there's something to draw and draw it
    uint8_t out;
    if (ledChange || (FakeEinkET013TT1RenderQueue != nullptr &&
                      xQueueReceiveFromISR(FakeEinkET013TT1RenderQueue, &out, nullptr) == pdTRUE)) {
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, deviceScreen, NULL, NULL);
        SimDrawScreen();
        SimDrawLeds();
        SDL_RenderPresent(renderer);
    }

    return true;
}

void emscriptemLoop() { mainLoop(); }

void appMainTask(void *parameters) {
    app_main();
    vTaskDelete(NULL);
}

auto main(int argc, char *argv[]) -> int {
#if SIMULATOR_BLE
    auto callback = [](RemoteCommand command) { ttCommand = command; };
    BleFindTurnTouch(TT_ADDRESS, callback);
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "Failed SDL_Init " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_CreateWindowAndRenderer(EinkET013TT1_PIXEL_WIDTH * SCALE,
                                ((EinkET013TT1_PIXEL_HEIGHT + SIM_LED_DIM) * SCALE), 0, &window,
                                &renderer);
    if (renderer == NULL) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return 1;
    }

    deviceScreen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                     EinkET013TT1_PIXEL_WIDTH, EinkET013TT1_PIXEL_HEIGHT);
    SDL_RenderSetScale(renderer, SCALE, SCALE);

#if defined(EMSCRIPTEN)
    emscripten_set_main_loop(emscriptemLoop, 0, 0);
#else

    // Pre-initialize PreferencesStore to point to simprefs.ini
    prefs.begin("simprefs.ini");

    // The FreeRTOS tick handler happens via SIGALRM, and it also kicks off preemptive task
    // switching, which is not what we want on the main thread, since it's not a real FreeRTOS task.
    // Just disable SIGALRM on this thread so that it only happens on FreeRTOS tasks.
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    // Start the FreeRTOS scheduler
    std::thread schedulerThread([]() {
        // ESP-IDF does `app_main` in a task as well. We need to do this so we can use
        // FreeRTOS-timed stuff like vTaskDelay
        xTaskCreate(appMainTask, "app_main", 10000, NULL, 1, NULL);
        vTaskStartScheduler();
        log_i("Scheduler thread done");
    });

    while (mainLoop()) {
        // Limit to ~60fps so we don't murder battery unnecessarily
        usleep(1000000.0 / 60.0);
    }

    log_i("Shutting down");

    vTaskEndScheduler();
    schedulerThread.join();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    log_i("Quit");
#endif

    return 0;
}
