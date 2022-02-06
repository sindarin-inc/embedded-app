#if SIMULATOR

#include "DisplaySDLSimulator.hpp"

#include <SDL2/SDL.h>

// TODO: Is there a better way to get the renderer here?
extern SDL_Renderer *renderer;

DisplaySDLSimulator::DisplaySDLSimulator()
    : DisplaySystem(DisplaySDL_X_PIXELS, DisplaySDL_Y_PIXELS) {

    setFullWindow();

    widthInMm_ = 23.3;
    heightInMm_ = 23.3;
}

void DisplaySDLSimulator::updateDisplay() {
    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            auto pixel = getPixel(x, y);
            uint8_t color = pixel ? 0xFF : 0x00;
            SDL_SetRenderDrawColor(renderer, color, color, color, 255);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
    SDL_RenderPresent(renderer);
    fillScreen(bg);
};

#endif
