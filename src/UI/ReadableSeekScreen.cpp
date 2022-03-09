#include "ReadableSeekScreen.hpp"

#include "Misc/StringUtil.hpp"
#include "UI/Bitmaps/SeekBook.hpp"

void ReadableSeekScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    auto autoUpdate = display.getAutoUpdate();
    display.setAutoUpdate(false);

    auto place = prefs_.readablePlace.get(readable_.id.c_str());
    auto progress = 100 * place / renderer_.size();
    auto percentStr = StringFormat("%d%%", progress);

    int16_t spacing = 8;
    int16_t barHeight = 10;
    int16_t x = (display.width() - Seekbook_width) / 2;
    // 96 is a pre-calculated height.
    // TODO: Do this calculation dynamically, once we have a way to gracefully layout subviews
    int16_t y = (display.height() - 96) / 2;

    display.drawXBitmap(x, y, seekbookBits, Seekbook_width, Seekbook_height, display.fg);
    y += Seekbook_height + spacing;

    uint16_t percentWidth, percentHeight;
    // Just pick a number for measurement. We don't want the UI moving when the number changes
    display.getTextBounds("50%", &percentWidth, &percentHeight);

    int16_t barWidth = 60;
    x = (display.width() - barWidth - percentWidth - spacing) / 2;
    display.drawRect(x, y, barWidth, barHeight, display.fg);
    int16_t progressWidth = progress * barWidth / 100;
    display.fillRect(x, y, progressWidth, barHeight, display.fg);

    x += barWidth + spacing;
    display.setCursor(x, y + display.getTextHeight() - 2);
    display.println(percentStr.c_str());

    // Cursor is from the baseline of the text I think
    y += barHeight + spacing + display.getTextHeight();
    x = 0;
    uint16_t textWidth, textHeight;
    auto message1 = "Press right and left";
    display.getTextBounds(message1, &textWidth, &textHeight);
    display.setCursor((display.width() - textWidth) / 2, y);
    display.println(message1);

    y += display.getTextHeight();
    auto message2 = "to seek through";
    display.getTextBounds(message2, &textWidth, &textHeight);
    display.setCursor((display.width() - textWidth) / 2, y);
    display.println(message2);

    y += display.getTextHeight();
    auto message3 = "readable";
    display.getTextBounds(message3, &textWidth, &textHeight);
    display.setCursor((display.width() - textWidth) / 2, y);
    display.println(message3);

    // up arrow
    uint16_t arrowWidth = 36, arrowHeight = 7;
    x = (display.width() - arrowWidth) / 2;
    y = 0;
    display.drawLine(x, y, x + arrowWidth / 2, y - arrowHeight, display.fg);
    display.drawLine(x + arrowWidth / 2, y - arrowHeight, x + arrowWidth, y, display.fg);

    display.updateDisplay(firstLoad ? DisplaySystem::UpdateType::FULL
                                    : DisplaySystem::UpdateType::FAST);
    firstLoad = false;

    display.setAutoUpdate(autoUpdate);

    needsRerender = false;
}

void ReadableSeekScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS: {
        // Do the -5% thing. This is unlikley to be right long-term
        // but it's way better if you overshoot.
        // TODO: Do we need placePrev anymore except for logging?
        readable_.placePrev = readable_.place;

        size_t place = readable_.place;
        size_t lengthOfFile = renderer_.size();

        // Subtract 5% to the file
        if (readable_.place >= lengthOfFile / 20) {
            readable_.place = readable_.place - lengthOfFile / 20;
        } else {
            readable_.place = 0;
        }

        log_d("Setting place from %d to %d", readable_.placePrev, readable_.place);

        renderer_.setCurrentPlace(readable_.place);
        prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
        needsRerender = true;
        break;
    }
    case RemoteCommand::CHANNEL_PLUS: {
        // TODO: Do we need placePrev anymore except for logging?
        readable_.placePrev = readable_.place;

        size_t place = renderer_.getCurrentPlace();
        size_t lengthOfFile = renderer_.size();

        // Add 5% to the file
        readable_.place = readable_.place + lengthOfFile / 20;

        log_d("Setting place from %d to %d", readable_.placePrev, readable_.place);

        // Don't set place past EOF
        if (readable_.place >= lengthOfFile) {
            readable_.place = lengthOfFile - 1;
        }

        renderer_.setCurrentPlace(readable_.place);
        prefs_.readablePlace.set(readable_.id.c_str(), readable_.place);
        needsRerender = true;
        break;
    } break;
    case RemoteCommand::PREV: {
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    }
    case RemoteCommand::NEXT: {
        break;
    }
    case RemoteCommand::BACK:
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    default:
        break;
    }
}
