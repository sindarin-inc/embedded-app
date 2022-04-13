#if !defined(EMSCRIPTEN)

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

#include "Inputs/RemoteInput.hpp"

// It will be cleaner to adopt the FreeRTOS POSIX port since then we don't have to work around
// the multitasking architecture
// https://www.freertos.org/FreeRTOS-simulator-for-Linux.html
extern void UIInnerLoop(RemoteCommand command); // NOLINT(readability-identifier-naming)
extern "C" void app_main(void);                 // NOLINT(readability-identifier-naming)

#define SCALE 2
#define WIDTH 256
#define HEIGHT 256

SDL_Renderer *renderer;
// Texture on which to draw the UI from the firmware
SDL_Texture *deviceScreen;
bool needsRedraw = true;

// Variables related to drawing the overlay
SDL_Surface *arrow;
SDL_Texture *arrowTexture;
Uint8 arrowOpacity = 0;
double arrowRotation = 0;

bool saveScreenshotPng(std::string filepath, SDL_Window *window, SDL_Renderer *renderer) {
    // 4 bytes per pixel
    auto pitch = WIDTH * 4;

    SDL_Surface *saveSurface = SDL_CreateRGBSurface(0, WIDTH, HEIGHT, 32, 0, 0, 0, 0);
    if (saveSurface == NULL) {
        std::cerr << "SDL_CreateRGBSurface error: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_Rect rect = {.x = 0, .y = 0, .w = WIDTH, .h = HEIGHT};
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
    }
    if (arrowOpacity > 0) {
        arrowOpacity = std::max(arrowOpacity - 10, 0);
        // The left arrow default position
        SDL_Rect left = {.x = 5, .y = (HEIGHT - arrow->h) / 2, .w = arrow->w, .h = arrow->h};
        // The center of the screen, relative to left arrow coordinates
        SDL_Point center = {.x = WIDTH / 2 - left.x, .y = arrow->h / 2};
        SDL_SetTextureAlphaMod(arrowTexture, arrowOpacity);
        SDL_RenderCopyEx(renderer, arrowTexture, NULL, &left, arrowRotation, &center,
                         SDL_FLIP_NONE);
    }
}

auto main(int argc, char *argv[]) -> int {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "Failed SDL_Init " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window;
    SDL_CreateWindowAndRenderer(WIDTH * SCALE, WIDTH * SCALE, 0, &window, &renderer);
    if (renderer == NULL) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return 1;
    }

    deviceScreen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                     WIDTH, HEIGHT);
    SDL_RenderSetScale(renderer, SCALE, SCALE);

    IMG_Init(IMG_INIT_PNG);
    arrow = IMG_Load("lib/Simulator/arrow.png");
    if (arrow == NULL) {
        fprintf(stderr, "Could not find arrow asset: %s\n", SDL_GetError());
        return 1;
    }
    arrowTexture = SDL_CreateTextureFromSurface(renderer, arrow);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    app_main();
    // Run the loop once to render the screen
    UIInnerLoop(RemoteCommand::NONE);

    SDL_Event e;
    bool quit = false;
    while (!quit) {
        RemoteCommand remoteCode = RemoteCommand::NONE;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
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
                }
                if (remoteCode != RemoteCommand::NONE) {
                    UIInnerLoop(remoteCode);
                    needsRedraw = true;
                }
            }
        }

        if (needsRedraw || arrowOpacity > 0) {
            SDL_SetRenderTarget(renderer, NULL);
            SDL_RenderCopy(renderer, deviceScreen, NULL, NULL);
            drawOverlay(remoteCode);
            SDL_RenderPresent(renderer);
        }

        // Limit to ~60fps so we don't murder battery unnecessarily
        usleep(1000000.0 / 60.0);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Quit" << std::endl;

    return 0;
}

#endif
