#include "UI/ReadableScreen.hpp"

#include "sindarin-debug.h"

void ReadableScreen::render() {
    if (file_renderer != nullptr && file_renderer->get_read_mode() == PAGE_READ) {
        readable.place_screed = file_renderer->render_continue_screed();
    }

    if (!needs_rerender) {
        return;
    }
    log_d("Rendering some text in READ MODE");
    new_place = file_renderer->render_page_with_box(readable.place);
    log_d("%d is the new place in the text", new_place);
    needs_rerender = false;
}

void ReadableScreen::process_command(RemoteCommand command) {
    auto back = [this]() {
        if (file_renderer != nullptr && file_renderer->get_read_mode() == SCREED_READ) {
            String prefs_prev_name = String(readable.id) + "-pr-p";
            String prefs_name = String(readable.id) + "-p";
            readable.place_prev = readable.place;
            readable.place = readable.place_screed;
            prefs.prefs.putULong(prefs_name.c_str(), readable.place);
            prefs.prefs.putULong(prefs_prev_name.c_str(), readable.place_prev);
        }

        if (back_callback != nullptr) {
            back_callback();
        }
        needs_rerender = true;
    };

    switch (command) {
    case RemoteCommand::CHANNEL_MINUS: {
        String prefs_name = String(readable.id) + "-pr-p";
        readable.place = file_renderer->get_previous_place();
        prefs.prefs.putULong(prefs_name.c_str(), readable.place);
        needs_rerender = true;
        break;
    }
    case RemoteCommand::CHANNEL_PLUS: {
        String prefs_prev_name = String(readable.id) + "-pr-p";
        String prefs_name = String(readable.id) + "-p";

        readable.place_prev = readable.place;
        readable.place = new_place;
        prefs.prefs.putULong(prefs_name.c_str(), readable.place);
        prefs.prefs.putULong(prefs_prev_name.c_str(), readable.place_prev);
        needs_rerender = true;
        break;
    }
    case RemoteCommand::PREV:
        if (file_renderer->get_read_mode() == SCREED_READ) {
            file_renderer->increase_speed();
        }
        back();
        break;
    case RemoteCommand::NEXT:
        if (file_renderer->get_read_mode() == SCREED_READ) {
            file_renderer->decrease_speed();
        }
        needs_rerender = true;
        break;
    case RemoteCommand::BACK:
        back();
        break;
    case RemoteCommand::NOP_BUTTON:
        if (file_renderer->get_read_mode() == SCREED_READ) {
            String prefs_prev_name = String(readable.id) + "-pr-p";
            String prefs_name = String(readable.id) + "-p";
            readable.place_prev = readable.place;
            readable.place = readable.place_screed;
            prefs.prefs.putULong(prefs_name.c_str(), readable.place);
            prefs.prefs.putULong(prefs_prev_name.c_str(), readable.place_prev);
        }
        file_renderer->flip_read_mode();
        needs_rerender = true;
        break;
    default:
        break;
    }
}