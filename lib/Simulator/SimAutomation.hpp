#include <SDL2/SDL.h>
#include <string>

#include "Displays/DisplaySystem.hpp"

bool saveScreenshotPng(DisplaySystem &display, std::string filepath, SDL_Window *window,
                       SDL_Renderer *renderer);

// Only press 'u' when you are on a page in a readable.
void runBackwardsUnitTest(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer);

// This is used to save all png images of a book in support of the
// automated testing capability
void saveBookImages(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer);

// Only press 'a' when you are on a page in a readable.
void previousPageFast(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer);

// Only press 's' when you are on a page in a readable.
void nextPageFast(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer);

void goBackNumPages(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer,
                    int numPages);

void goBackwardNumPages(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer,
                        int numPages);

void goForwardNumPages(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer,
                       int numPages);