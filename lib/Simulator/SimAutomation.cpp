#include "SimAutomation.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#pragma clang diagnostic pop
#include <Defer/Defer.hpp>
#include <unistd.h>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Dispatch.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/Screen.hpp"

// From main.cpp
extern void UIInnerLoop(RemoteCommand command);
extern auto UIGetCurrentScreen() -> std::shared_ptr<Screen>;

// From native.cpp
extern void SimDrawScreen();
extern void SimDrawLeds();
extern SDL_Texture *deviceScreen;

bool saveScreenshotPng(DisplaySystem &display, std::string filepath, SDL_Window *window,
                       SDL_Renderer *renderer) {
    if (!stbi_write_png(filepath.c_str(), display.gfxClass()->width(), display.gfxClass()->height(),
                        1, display.buffer().get(), 0)) {
        log_e("Unable to save rendered image to %s", filepath.c_str());
    }

    return true;
}

bool isPngEqual(std::string filepath1, std::string filepath2) {
    int width1, height1, bpp1;
    int width2, height2, bpp2;

    uint8_t *img1 = stbi_load(filepath1.c_str(), &width1, &height1, &bpp1, 3);
    uint8_t *img2 = stbi_load(filepath2.c_str(), &width2, &height2, &bpp2, 3);

    if (img1 == nullptr || img2 == nullptr) {
        std::cerr << "Failed to load one or both images\n";
        return false;
    }

    if (width1 != width2 || height1 != height2 || bpp1 != bpp2) {
        std::cerr << "Images are not the same size or color depth\n";
        stbi_image_free(img1);
        stbi_image_free(img2);
        return false;
    }

    for (int y = 0; y < height1; y++) {
        for (int x = 0; x < width1; x++) {
            int idx = (y * width1 + x) * 3;
            if (img1[idx] != img2[idx] || img1[idx + 1] != img2[idx + 1] ||
                img1[idx + 2] != img2[idx + 2]) {
                std::cerr << "Pixel difference at " << x << ", " << y << "\n";
                std::cerr << "Image 1: " << (int)img1[idx] << " " << (int)img1[idx + 1] << " "
                          << (int)img1[idx + 2] << "\n";
                std::cerr << "Image 2: " << (int)img2[idx] << " " << (int)img2[idx + 1] << " "
                          << (int)img2[idx + 2] << "\n";
                stbi_image_free(img1);
                stbi_image_free(img2);
                return false;
            }
        }
    }

    stbi_image_free(img1);
    stbi_image_free(img2);

    return true;
}

void sendCodeAndRender(DisplaySystem &display, RemoteCommand command, SDL_Renderer *renderer) {
    DispatchGetUiQueue()->dispatchFromISR([command]() { UIInnerLoop(command); });

    // Wait for the UI thread to do the rendering to be rendered
    uint8_t out;
    while (xQueueReceiveFromISR(display.renderQueue(), &out, nullptr) == pdFALSE) {
        usleep(10000);
    }

    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, deviceScreen, NULL, NULL);
    SimDrawScreen();
    SimDrawLeds();
    SDL_RenderPresent(renderer);
}

// Only press 'u' when you are on a page in a readable.
void runBackwardsUnitTest(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer) {
    auto skipWaveform = display.getSkipWaveform();
    display.setSkipWaveform(true);
    defer { display.setSkipWaveform(skipWaveform); };

    auto screen = UIGetCurrentScreen();
    auto readableScreen = std::dynamic_pointer_cast<ReadableScreen>(screen);

    if (readableScreen != nullptr) {
        bool first = true;
        for (int i = 0;; i++) {
            if (!first) {
                saveScreenshotPng(display, "current.png", window, renderer);
                if (readableScreen->checkAtEnd()) break;
                sendCodeAndRender(display, RemoteCommand::ENTER, renderer);
            }
            first = false;
            saveScreenshotPng(display, "next.png", window, renderer);
            if (readableScreen->checkAtEnd()) break;
            sendCodeAndRender(display, RemoteCommand::ENTER, renderer);
            saveScreenshotPng(display, "nextnext.png", window, renderer);
            sendCodeAndRender(display, RemoteCommand::LEFT, renderer);
            saveScreenshotPng(display, "nextprev.png", window, renderer);
            // compare the most two recent pngs -- they should be the same.
            // if not, error out.
            if (!isPngEqual("./next.png", "./nextprev.png")) {
                std::cerr << "images don't match, sad face\n";
                std::cerr << "check out next.png and nextprev.png for the diffs\n";
                exit(1);
            }
        }
        std::cerr << "Unit test passed\n";
    } else {
        std::cerr << "Not in a readableScreen!!\n";
    }
}

// This is used to save all png images of a book in support of the
// automated testing capability
void saveBookImages(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer) {
    auto skipWaveform = display.getSkipWaveform();
    display.setSkipWaveform(true);
    defer { display.setSkipWaveform(skipWaveform); };

    auto screen = UIGetCurrentScreen();
    auto readableScreen = std::dynamic_pointer_cast<ReadableScreen>(screen);

    if (readableScreen != nullptr) {
        for (int i = 0;; i++) {
            std::ostringstream fname;
            fname << "bookImages/" << i << ".png";
            saveScreenshotPng(display, fname.str(), window, renderer);
            if (readableScreen->checkAtEnd()) break;
            sendCodeAndRender(display, RemoteCommand::ENTER, renderer);
        }
        std::cerr << "Save Book Image Completed!" << std::endl;
    } else {
        std::cerr << "Not in a ReadableScreen!" << std::endl;
    }
}

// Only press 'a' when you are on a page in a readable.
void previousPageFast(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer) {
    auto skipWaveform = display.getSkipWaveform();
    display.setSkipWaveform(true);
    defer { display.setSkipWaveform(skipWaveform); };

    sendCodeAndRender(display, RemoteCommand::LEFT, renderer);

    std::cerr << "unit test passed\n";
}

// Only press 's' when you are on a page in a readable.
void nextPageFast(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer) {
    auto skipWaveform = display.getSkipWaveform();
    display.setSkipWaveform(true);
    defer { display.setSkipWaveform(skipWaveform); };

    sendCodeAndRender(display, RemoteCommand::ENTER, renderer);

    std::cerr << "unit test passed\n";
}

void goBackNumPages(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer,
                    int numPages) {
    auto skipWaveform = display.getSkipWaveform();
    display.setSkipWaveform(true);
    defer { display.setSkipWaveform(skipWaveform); };

    int i = 0;
    for (i = 0; i < numPages; i++) {
        sendCodeAndRender(display, RemoteCommand::LEFT, renderer);
        usleep(1000000.0 / 240.0);
    }
}

void goBackwardNumPages(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer,
                        int numPages) {
    auto skipWaveform = display.getSkipWaveform();
    display.setSkipWaveform(true);
    defer { display.setSkipWaveform(skipWaveform); };

    while (numPages--) {
        sendCodeAndRender(display, RemoteCommand::LEFT, renderer);
        usleep(1000000.0 / 1.5);
    }
}

void goForwardNumPages(DisplaySystem &display, SDL_Window *window, SDL_Renderer *renderer,
                       int numPages) {
    auto skipWaveform = display.getSkipWaveform();
    display.setSkipWaveform(true);
    defer { display.setSkipWaveform(skipWaveform); };

    while (numPages--) {
        sendCodeAndRender(display, RemoteCommand::RIGHT, renderer);
        usleep(1000000.0 / 1.5);
    }
}
