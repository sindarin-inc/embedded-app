#if !defined(EMSCRIPTEN)

#include <SDL2/SDL.h>
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
    unsigned int lastRunTime = SDL_GetTicks();
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
                }
            }
        }

        // Frame limit so we don't murder battery unnecessarily
        unsigned int now = SDL_GetTicks();
        double delta = now - lastRunTime;
        if (delta > 1000 / 60.0) {
            lastRunTime = now;
            loop();
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Quit" << std::endl;

    return 0;
}

#endif
