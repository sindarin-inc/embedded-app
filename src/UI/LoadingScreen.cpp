#include "UI/LoadingScreen.hpp"

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

#define INTERVAL 1000
#define BOX_WIDTH 10
#define SPACING 5

void LoadingScreen::render() {
    auto now = millis();
    if (needs_rerender || (now - timer > INTERVAL)) {
        // TODO: Would be nice to selectively clear parts of the screen
        if (state == 0) {
            renderer->render_max_box();
        }
        int width = renderer->get_pixel_width();
        int height = renderer->get_pixel_height();
        int startX = (width / 2) - (3 * BOX_WIDTH + 2 * SPACING) / 2;
        int xOffset = state * (BOX_WIDTH + SPACING);
        int startY = (height / 2) - BOX_WIDTH / 2;
        renderer->render_box(startX + xOffset, startY, startX + xOffset + BOX_WIDTH,
                             startY + BOX_WIDTH);
        state++;
        state = state % 3;
        timer = now;
        needs_rerender = false;
    }
}

void LoadingScreen::process_command(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::BACK:
        if (back_callback != nullptr) {
            back_callback();
        }
        break;
    default:
        break;
    }
}
