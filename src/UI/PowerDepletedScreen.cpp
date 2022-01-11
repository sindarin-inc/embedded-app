#include "PowerDepletedScreen.hpp"

#include "sindarin-debug.h"

void PowerDepletedScreen::render() {
    if (!needs_rerender) {
        return;
    }
    log_d("Rendering PowerDepletedScreen");

    String message;
    // TODO: Add design!
    message = "Battery depleted\n";
    renderer.render_max_box_with_text(message);
    needs_rerender = false;
}

void PowerDepletedScreen::process_command(RemoteCommand command) {}
