#include "UI/RawRendererScreen.hpp"

#include "sindarin-debug.h"

void RawRendererScreen::render() {
    if (renderer != nullptr && renderer->get_read_mode() == PAGE_READ) {
        renderer->render_continue_screed();
    }

    if (!needs_rerender) {
        return;
    }
    log_d("Rendering some text in READ MODE");
    new_place = renderer->render_page_with_box(new_place);
    log_d("%d is the new place in the text", new_place);
    needs_rerender = false;
}

void RawRendererScreen::process_command(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS: {
        new_place = prev_place;
        needs_rerender = true;
        break;
    }
    case RemoteCommand::CHANNEL_PLUS: {
        prev_place = new_place;
        needs_rerender = true;
        break;
    }
    case RemoteCommand::PREV:
        if (renderer->get_read_mode() == SCREED_READ) {
            renderer->increase_speed();
        }
        needs_rerender = true;
        break;
    case RemoteCommand::NEXT:
        if (renderer->get_read_mode() == SCREED_READ) {
            renderer->decrease_speed();
        }
        needs_rerender = true;
        break;
    case RemoteCommand::BACK:
        if (back_callback != nullptr) {
            back_callback();
        }
        break;
    case RemoteCommand::NOP_BUTTON:
        renderer->flip_read_mode();
        needs_rerender = true;
        break;
    default:
        break;
    }
}
