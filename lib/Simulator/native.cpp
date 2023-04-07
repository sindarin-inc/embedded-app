#if defined(EMSCRIPTEN)
#include <emscripten.h>
#endif
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iostream>
#include <signal.h>
#include <thread>
#include <unistd.h>

#include "Displays/FakeEinkET013TT1.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Math.hpp"
#include "Sensors/LightingController.hpp"
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

extern void UIInnerLoop(RemoteCommand command);
extern "C" void app_main(void); // NOLINT(readability-identifier-naming)

const char *kGitHash = "SIMULATOR";

#define SCALE 2

// Will be set by PlatformSimulator.cpp SetLeftScreenLight() and SetRightScreenLight()
extern int LightingSetting;
extern bool simLeds[3];
extern bool ledChange;

SDL_Window *window;
SDL_Renderer *renderer;
// Texture on which to draw the UI from the firmware
SDL_Texture *deviceScreen;

// Variables related to drawing the overlay
SDL_Surface *arrow;
SDL_Texture *arrowTexture;
Uint8 arrowOpacity = 0;
double arrowRotation = 0;

float lerp(float a, float b, float t) { return a + t * (b - a); }

void SimDrawScreen() {
    for (int x = 0; x < EinkET013TT1_PIXEL_WIDTH; x++) {
        for (int y = 0; y < EinkET013TT1_PIXEL_HEIGHT; y++) {
            uint8_t color = FakeEinkET013TT1ParticlePositions[x + y * EinkET013TT1_PIXEL_WIDTH];
            float colorF = color / 255.0;
            uint16_t brightnessOffset =
                MapRange(LightingSetting, MIN_LIGHTING_DUTY_CYCLE, MAX_LIGHTING_DUTY_CYCLE, 60, 0);
            SDL_SetRenderDrawColor(
                renderer, lerp(40 + brightnessOffset, 248 - brightnessOffset, colorF),
                lerp(40 + brightnessOffset, 240 - brightnessOffset, colorF),
                lerp(40 + brightnessOffset, 227 - brightnessOffset, colorF), 0xFF);
            SDL_RenderDrawPoint(renderer, x, y);
        }
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
            float dist = sqrt(dx*dx + dy*dy);
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
    int ledStartX = (EinkET013TT1_PIXEL_WIDTH/2) - (ledDim/2);

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
    SDL_Surface *saveSurface1 = IMG_Load(filepath1.c_str());
    SDL_Surface *saveSurface2 = IMG_Load(filepath2.c_str());

    for (int w = 0; w < EinkET013TT1_PIXEL_WIDTH; w++) {
        for (int h = 0; h < EinkET013TT1_PIXEL_HEIGHT; h++) {
            Uint8 r1, g1, b1, r2, g2, b2;
            Uint32 *pixels1, *pixels2;
            pixels1 = (Uint32 *)saveSurface1->pixels;
            pixels2 = (Uint32 *)saveSurface2->pixels;
            Uint32 pixel1 = pixels1[w + h * saveSurface1->w];
            Uint32 pixel2 = pixels2[w + h * saveSurface2->w];
            SDL_GetRGB(pixel1, saveSurface1->format, &r1, &g1, &b1);
            SDL_GetRGB(pixel2, saveSurface2->format, &r2, &g2, &b2);
            if ((r1 != r2) || (g1 != g2) || (b1 != b2)) {
                std::cerr << "Got here and something went wrong " << w << " " << h << "\n";
                std::cerr << r1 << " " << g1 << " " << b1 << "\n";
                std::cerr << r2 << " " << g2 << " " << b2 << "\n";
                return false;
            }
        }
    }
    return true;
}

bool saveScreenshotPng(std::string filepath, SDL_Window *window, SDL_Renderer *renderer) {
    // 4 bytes per pixel
    auto pitch = EinkET013TT1_PIXEL_WIDTH * 4;

    SDL_Surface *saveSurface = SDL_CreateRGBSurface(0, EinkET013TT1_PIXEL_WIDTH,
                                                    EinkET013TT1_PIXEL_HEIGHT, 32, 0, 0, 0, 0);
    if (saveSurface == NULL) {
        std::cerr << "SDL_CreateRGBSurface error: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_Rect rect = {.x = 0, .y = 0, .w = EinkET013TT1_PIXEL_WIDTH, .h = EinkET013TT1_PIXEL_HEIGHT};
    SDL_SetRenderTarget(renderer, deviceScreen);
    if (SDL_RenderReadPixels(renderer, &rect, 0, saveSurface->pixels, pitch) != 0) {
        std::cerr << "SDL_RenderReadPixels error: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_SetRenderTarget(renderer, NULL);

    IMG_SavePNG(saveSurface, filepath.c_str());
    SDL_FreeSurface(saveSurface);

    return true;
}

void drawOverlay(RemoteCommand command) {
    if (command == RemoteCommand::PREV) {
        arrowOpacity = SDL_MAX_UINT8;
        arrowRotation = 90;
    } else if (command == RemoteCommand::NEXT) {
        arrowOpacity = SDL_MAX_UINT8;
        arrowRotation = 270;
    } else if (command == RemoteCommand::CHANNEL_MINUS) {
        arrowOpacity = SDL_MAX_UINT8;
        arrowRotation = 0;
    } else if (command == RemoteCommand::CHANNEL_PLUS) {
        arrowOpacity = SDL_MAX_UINT8;
        arrowRotation = 180;
    } else if (command == RemoteCommand::ENTER) {
        arrowOpacity = SDL_MAX_UINT8;
        arrowRotation = 225;
    }
    if (arrowOpacity > 0) {
        arrowOpacity = std::max(arrowOpacity - 10, 0);
        // The left arrow default position
        SDL_Rect left = {
            .x = 5, .y = (EinkET013TT1_PIXEL_HEIGHT - arrow->h) / 2, .w = arrow->w, .h = arrow->h};
        // The center of the screen, relative to left arrow coordinates
        SDL_Point center = {.x = EinkET013TT1_PIXEL_WIDTH / 2 - left.x, .y = arrow->h / 2};
        SDL_SetTextureAlphaMod(arrowTexture, arrowOpacity);
        SDL_RenderCopyEx(renderer, arrowTexture, NULL, &left, arrowRotation, &center,
                         SDL_FLIP_NONE);
    }
}

#if SIMULATOR_BLE
// BLE command from the TurnTouch remote
RemoteCommand ttCommand;
#endif

void sendCodeAndRender(RemoteCommand command, SDL_Renderer *renderer) {
    DispatchGetUiQueue()->dispatchFromISR([command]() { UIInnerLoop(command); });

    // Wait for the UI thread to do the rendering to be rendered
    uint8_t out;
    while (xQueueReceiveFromISR(FakeEinkET013TT1DoneWaveformQueue, &out, nullptr) == pdFALSE) {
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
    for (int i = 0; i < 500; i++) {
        saveScreenshotPng("current.png", window, renderer);
        sendCodeAndRender(RemoteCommand::ENTER, renderer);
        saveScreenshotPng("next.png", window, renderer);
        sendCodeAndRender(RemoteCommand::ENTER, renderer);
        saveScreenshotPng("nextnext.png", window, renderer);
        sendCodeAndRender(RemoteCommand::CHANNEL_MINUS, renderer);
        saveScreenshotPng("nextprev.png", window, renderer);
        // compare the most two recent pngs -- they should be the same.
        // if not, error out.
        if (!isPngEqual("./next.png", "./nextprev.png")) {
            std::cerr << "images don't match, sad face\n";
            std::cerr << "check out next.png and nextprev.png for the diffs\n";
            exit(1);
        }
    }
    std::cerr << "unit test passed\n";
}

void goBackNumPages(SDL_Window *window, SDL_Renderer *renderer, int numPages) {
    int i = 0;

    for (i = 0; i < numPages; i++) {
        sendCodeAndRender(RemoteCommand::CHANNEL_MINUS, renderer);
        usleep(1000000.0 / 240.0);
    }
}

// Returns false if the program should quit
auto mainLoop() -> bool {
    SDL_Event e;
    RemoteCommand remoteCode = RemoteCommand::NONE;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return false;
        } else if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
            case SDLK_UP:
                remoteCode = RemoteCommand::PREV;
                break;
            case SDLK_DOWN:
                remoteCode = RemoteCommand::NEXT;
                break;
            case SDLK_LEFT:
                remoteCode = RemoteCommand::CHANNEL_MINUS;
                break;
            case SDLK_RIGHT:
                remoteCode = RemoteCommand::CHANNEL_PLUS;
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
            case 'u':
                runBackwardsUnitTest(window, renderer);
                break;
            case 'b':
                goBackNumPages(window, renderer, 50);
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
    if (FakeEinkET013TT1RenderQueue != nullptr &&
        xQueueReceiveFromISR(FakeEinkET013TT1RenderQueue, &out, nullptr) == pdTRUE) {
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, deviceScreen, NULL, NULL);
        SimDrawScreen();
        SimDrawLeds();
        SDL_RenderPresent(renderer);
    } else if (ledChange) {
        #if SIM_LED_ON
        SDL_SetRenderTarget(renderer, NULL);
        SimDrawLeds();
        SDL_RenderPresent(renderer);
        #endif
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

    SDL_CreateWindowAndRenderer(EinkET013TT1_PIXEL_WIDTH * SCALE, ((EinkET013TT1_PIXEL_WIDTH+SIM_LED_DIM) * SCALE),
                                0, &window, &renderer);
    if (renderer == NULL) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return 1;
    }

    deviceScreen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                     EinkET013TT1_PIXEL_WIDTH, EinkET013TT1_PIXEL_HEIGHT);
    SDL_RenderSetScale(renderer, SCALE, SCALE);

    IMG_Init(IMG_INIT_PNG);
    arrow = IMG_Load("lib/Simulator/assets/arrow.png");
    if (arrow == NULL) {
        fprintf(stderr, "Could not find arrow asset: %s\n", SDL_GetError());
        return 1;
    }
    arrowTexture = SDL_CreateTextureFromSurface(renderer, arrow);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

#if defined(EMSCRIPTEN)
    emscripten_set_main_loop(emscriptemLoop, 0, 0);
#else

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
