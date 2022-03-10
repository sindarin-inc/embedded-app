#if !defined(EMSCRIPTEN)

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

extern void setup();            // NOLINT(readability-identifier-naming)
extern void loop();             // NOLINT(readability-identifier-naming)
extern int SimulatorRemoteCode; // NOLINT(readability-identifier-naming)

#define SCALE 2
#define WIDTH 256
#define HEIGHT 256

// TODO: It'd be nice to not duplicate this
enum RemoteCommand : int {
    VOL_DOWN = 0x0,
    PLAY = 0x1,
    VOL_UP = 0x2,
    SETUP = 0x4,
    PREV = 0x5,
    STOP = 0x6,
    CHANNEL_MINUS = 0x8,
    ENTER = 0x9,
    CHANNEL_PLUS = 0xA,
    NOP_BUTTON = 0xC,
    NEXT = 0xD,
    BACK = 0xE,
    FACTORY_DEFAULTS = 0xFF,
    NONE = -1,
};

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

    setup();

    SDL_Event e;
    bool quit = false;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_UP:
                    SimulatorRemoteCode = PREV;
                    break;
                case SDLK_DOWN:
                    SimulatorRemoteCode = NEXT;
                    break;
                case SDLK_LEFT:
                    SimulatorRemoteCode = CHANNEL_MINUS;
                    break;
                case SDLK_RIGHT:
                    SimulatorRemoteCode = CHANNEL_PLUS;
                    break;
                case SDLK_ESCAPE:
                    SimulatorRemoteCode = BACK;
                    break;
                case SDLK_RETURN:
                    SimulatorRemoteCode = ENTER;
                    break;
                case 'p':
                    saveScreenshotPng("./screenshot.png", window, renderer);
                    break;
                }
            }
        }

        loop();
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
