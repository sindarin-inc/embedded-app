#if SIMULATOR

#include "DisplaySDLSimulator.hpp"

#include <SDL2/SDL.h>

extern SDL_Renderer *renderer;
extern SDL_Texture *deviceScreen;

DisplaySDLSimulator::DisplaySDLSimulator()
    : gfxCanvas_(DisplaySDL_X_PIXELS, DisplaySDL_Y_PIXELS), DisplaySystem() {
    gfxClass_ = &gfxCanvas_;
}

bool DisplaySDLSimulator::setup() {
    DisplaySystem::setup();
    setFullWindow();
    return true;
}

void DisplaySDLSimulator::updateDisplay(DisplaySystem::UpdateType updateType) {
    SDL_SetRenderTarget(renderer, deviceScreen);
    for (int x = 0; x < gfxCanvas_.width(); x++) {
        for (int y = 0; y < gfxCanvas_.height(); y++) {
            auto pixel = gfxCanvas_.getPixel(x, y);
            uint8_t color = pixel ? 0xFF : 0x00;
            uint16_t brightnessOffset = 3 * (21 - lighting_);
            if (color == 0xFF) {
                // Offwhite
                SDL_SetRenderDrawColor(renderer, 248 - brightnessOffset, 240 - brightnessOffset,
                                       227 - brightnessOffset, 0xFF);
            } else {
                // Offblack
                SDL_SetRenderDrawColor(renderer, 40 + brightnessOffset, 40 + brightnessOffset,
                                       40 + brightnessOffset, 0xFF);
            }
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, deviceScreen, NULL, NULL);
    SDL_RenderPresent(renderer);
    fillScreen(bg);
};

#endif
