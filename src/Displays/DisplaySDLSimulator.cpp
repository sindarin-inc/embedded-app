#if SIMULATOR

#include "DisplaySDLSimulator.hpp"

#include <SDL2/SDL.h>

// TODO: Is there a better way to get the renderer here?
extern SDL_Renderer *renderer;

DisplaySDLSimulator::DisplaySDLSimulator()
    : gfxCanvas_(DisplaySDL_X_PIXELS, DisplaySDL_Y_PIXELS), DisplaySystem() {
    gfxClass_ = &gfxCanvas_;

    widthInMm_ = 23.3;
    heightInMm_ = 23.3;
}

void DisplaySDLSimulator::setup() {
    DisplaySystem::setup();
    setFullWindow();
}

void DisplaySDLSimulator::updateDisplay() {
    for (int x = 0; x < gfxCanvas_.width(); x++) {
        for (int y = 0; y < gfxCanvas_.height(); y++) {
            auto pixel = gfxCanvas_.getPixel(x, y);
            uint8_t color = pixel ? 0xFF : 0x00;
            SDL_SetRenderDrawColor(renderer, color, color, color, 255);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
    SDL_RenderPresent(renderer);
    fillScreen(bg);
};

#endif
