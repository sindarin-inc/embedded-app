#include "UI/OffsetAdjustScreen.hpp"

#include "sindarin-debug.h"

void OffsetAdjustScreen::render() {
    if (!needs_rerender) {
        return;
    }
    log_d("Rendering");

    String menu_string;
    String bat_life;
    String total_to_display;
    menu_string = "Press arrows to move\nbox around to your\nliking. Then press\nenter "
                  "when done!\nHave a nice day.\n";
    bat_life = this->power.getBatteryLifeString();
    total_to_display = menu_string + String("\n\n") + bat_life;
    renderer.render_max_box_with_text(total_to_display);
    needs_rerender = false;
}

void OffsetAdjustScreen::process_command(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        prefs.local_x_offset--;
        needs_rerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        prefs.local_x_offset++;
        needs_rerender = true;
        break;
    case RemoteCommand::PREV:
        prefs.local_y_offset--;
        needs_rerender = true;
        break;
    case RemoteCommand::NEXT:
        prefs.local_y_offset++;
        needs_rerender = true;
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::ENTER:
        prefs.setLocalXOffset(prefs.local_x_offset);
        prefs.setLocalYOffset(prefs.local_y_offset);
        log_d("Saving x and y offset to preferences: (%d, %d)", prefs.local_x_offset,
              prefs.local_y_offset);
        back_callback();
        break;
    default:
        break;
    }
    renderer.local_x_offset = prefs.local_x_offset;
    renderer.local_y_offset = prefs.local_y_offset;
}
