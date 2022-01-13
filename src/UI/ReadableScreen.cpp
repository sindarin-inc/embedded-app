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
            readable.place_prev = readable.place;
            readable.place = readable.place_screed;
            prefs.readablePlace.set(readable.id, readable.place);
            prefs.readablePreviousPlace.set(readable.id, readable.place_prev);
        }

        if (back_callback != nullptr) {
            back_callback();
        }
        needs_rerender = true;
    };

    switch (command) {
    case RemoteCommand::CHANNEL_MINUS: {
        readable.place = file_renderer->get_previous_place();
        prefs.readablePreviousPlace.set(readable.id, readable.place);
        needs_rerender = true;
        break;
    }
    case RemoteCommand::CHANNEL_PLUS: {
        readable.place_prev = readable.place;
        readable.place = new_place;
        prefs.readablePlace.set(readable.id, readable.place);
        prefs.readablePreviousPlace.set(readable.id, readable.place_prev);
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
            readable.place_prev = readable.place;
            readable.place = readable.place_screed;
            prefs.readablePlace.set(readable.id, readable.place);
            prefs.readablePreviousPlace.set(readable.id, readable.place_prev);
        }
        file_renderer->flip_read_mode();
        needs_rerender = true;
        break;
    default:
        break;
    }
}