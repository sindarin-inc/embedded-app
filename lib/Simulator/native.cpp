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

bool saveScreenshotPng(std::string filepath, SDL_Window *window, SDL_Renderer *renderer) {
    auto bytesPerPixel = 4;
    auto pitch = WIDTH * SCALE * bytesPerPixel;

    SDL_Surface *saveSurface =
        SDL_CreateRGBSurface(0, WIDTH * SCALE, HEIGHT * SCALE, 32, 0, 0, 0, 0);
    if (saveSurface == NULL) {
        std::cerr << "SDL_CreateRGBSurface error: " << SDL_GetError() << "\n";
        return false;
    }
    // Seems like this reads from the back buffer (SDL is double buffered), so it gets the last
    // image, not the current one :/
    SDL_Rect rect = {.x = 0, .y = 0, .w = WIDTH * SCALE, .h = HEIGHT * SCALE};
    if (SDL_RenderReadPixels(renderer, &rect, 0, saveSurface->pixels, pitch) != 0) {
        std::cerr << "SDL_RenderReadPixels error: " << SDL_GetError() << "\n";
        return false;
    }

    IMG_SavePNG(saveSurface, filepath.c_str());
    SDL_FreeSurface(saveSurface);

    return true;
}

auto main(int argc, char *argv[]) -> int {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "Failed SDL_Init " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window;
    SDL_CreateWindowAndRenderer(WIDTH * SCALE, WIDTH * SCALE, 0, &window, &renderer);
    SDL_RenderSetScale(renderer, SCALE, SCALE);

    if (renderer == NULL) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return 1;
    }

    app_main();
    // Run the loop once to render the screen
    UIInnerLoop(RemoteCommand::NONE);

    SDL_Event e;
    bool quit = false;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_KEYDOWN) {
                RemoteCommand remoteCode = RemoteCommand::NONE;
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
                }
            }
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
