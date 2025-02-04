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

#include "Misc/Dispatch.hpp"
#include "Misc/Math.hpp"
#include "Misc/Platform.hpp"
#include "Sensors/LightingController.hpp"
#include "SimAutomation.hpp"
#include "Storage/PreferencesStore.hpp"
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

#if CONFIG_DISPLAY_EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#ifndef SIM_SCALE
#define SIM_SCALE 2
#endif // SIM_SCALE
extern DisplayEinkET013TT1 myDisplay;
#elif DISPLAY_SIM_8BIT
#ifndef SIM_WIDTH
#define SIM_WIDTH 600
#endif // SIM_WIDTH
#ifndef SIM_HEIGHT
#define SIM_HEIGHT 600
#endif // SIM_HEIGHT
#ifdef SIM_ONE_BIT
#include "Displays/DisplaySim1Bit.hpp"
DisplaySim1Bit myDisplay(SIM_WIDTH, SIM_HEIGHT);
#else
#include "Displays/DisplaySim8Bit.hpp"
DisplaySim8Bit myDisplay(SIM_WIDTH, SIM_HEIGHT);
#endif // SIM_ONE_BIT
#ifndef SIM_SCALE
#define SIM_SCALE 1
#endif // SIM_SCALE
#endif // CONFIG_DISPLAY_EINKET013TT1

extern "C" void app_main(void); // NOLINT(readability-identifier-naming)

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

void SimDrawScreen() {
    for (int x = 0; x < myDisplay.gfxClass()->width(); x++) {
        for (int y = 0; y < myDisplay.gfxClass()->height(); y++) {
#ifdef SIM_ONE_BIT
            SDL_SetRenderDrawColor(renderer, 240, 240, 240, 0xFF);

            if (myDisplay.getPixel(x, y)) {
                SDL_RenderDrawPoint(renderer, x, y);
            }
#else
            uint8_t color = myDisplay.buffer()[x + y * myDisplay.gfxClass()->width()];
            float colorF = color / 255.0;
            uint16_t brightnessOffset =
                MapRange(LightingSetting, static_cast<int>(GetMinLightingDutyCycle()),
                         static_cast<int>(GetMaxLightingDutyCycle()), 60, 0);
            SDL_SetRenderDrawColor(
                renderer, lerp(40 + brightnessOffset, 248 - brightnessOffset, colorF),
                lerp(40 + brightnessOffset, 240 - brightnessOffset, colorF),
                lerp(40 + brightnessOffset, 227 - brightnessOffset, colorF), 0xFF);
            SDL_RenderDrawPoint(renderer, x, y);
#endif
        }
    }
    if (movieOn_) {
        totalFrames++;
        char filename[100];
        snprintf(filename, sizeof(filename), "frames/frame-%05d.png", totalFrames);
        saveScreenshotPng(myDisplay, filename, window, renderer);
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
    int ledStartY = myDisplay.gfxClass()->height();
    int ledStartX = (myDisplay.gfxClass()->width() / 2) - (ledDim / 2);

    // clear area
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
    SDL_Rect rect;
    rect.x = 0;
    rect.y = ledStartY;
    rect.h = ledDim;
    rect.w = myDisplay.gfxClass()->width();

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

#if SIMULATOR_BLE
// BLE command from the TurnTouch remote
RemoteCommand ttCommand;
#endif

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
                saveScreenshotPng(myDisplay, "./screenshot.png", window, renderer);
                break;
            case 'm':
                movieOn_ = !movieOn_;
                break;
            case 'u':
                runBackwardsUnitTest(myDisplay, window, renderer);
                break;
            case 'b':
                goBackNumPages(myDisplay, window, renderer, 50);
                break;
            case 'a':
                previousPageFast(myDisplay, window, renderer);
                break;
            case 's':
                nextPageFast(myDisplay, window, renderer);
                break;
            case 'q':
                // Ignore cmd-q (quit on macOS)
                if (!(modState & KMOD_GUI)) {
                    goBackwardNumPages(myDisplay, window, renderer, 200);
                }
                break;
            case 'w':
                // Ignore cmd-w (close on macOS)
                if (!(modState & KMOD_GUI)) {
                    goForwardNumPages(myDisplay, window, renderer, 200);
                }
                break;
            case 'i':
                saveBookImages(myDisplay, window, renderer);
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
    if (ledChange || (myDisplay.renderQueue() != nullptr &&
                      xQueueReceiveFromISR(myDisplay.renderQueue(), &out, nullptr) == pdTRUE)) {
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

    SDL_CreateWindowAndRenderer(myDisplay.gfxClass()->width() * SIM_SCALE,
                                ((myDisplay.gfxClass()->height() + SIM_LED_DIM) * SIM_SCALE), 0,
                                &window, &renderer);
    if (renderer == NULL) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return 1;
    }

    deviceScreen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                     myDisplay.gfxClass()->width(), myDisplay.gfxClass()->height());
    SDL_RenderSetScale(renderer, SIM_SCALE, SIM_SCALE);

#if defined(EMSCRIPTEN)
    emscripten_set_main_loop(emscriptemLoop, 0, 0);
#else

    // Pre-initialize PreferencesStore to point to simprefs.ini
    prefs.begin("simprefs.ini");

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
