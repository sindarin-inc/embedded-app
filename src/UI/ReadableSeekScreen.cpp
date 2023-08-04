#include "ReadableSeekScreen.hpp"

#include "Misc/SolClock.hpp"
#include "Misc/StringUtil.hpp"
#include "UI/Bitmaps/SeekBook.hpp"

void ReadableSeekScreen::render(DisplaySystem &display) {
    auto place = Place::factory(prefs_.readablePlace.get(readable_->shortenedUuid.c_str()));
    auto progress = 100 * (float)place->getOffset() / (float)renderer_.size();

    auto percentStr = StringFormat("%.1f%%", progress);

    int16_t spacing = 8;
    int16_t barHeight = 10;
    int16_t x = (display.width() - Seekbook_width) / 2;
    // 96 is a pre-calculated height.
    // TODO: Do this calculation dynamically, once we have a way to gracefully layout subviews
    int16_t y = (display.height() - 96) / 2;

    display.drawXBitmap(x, y, SEEKBOOK_BITS, Seekbook_width, Seekbook_height, display.fg);
    y += Seekbook_height + spacing;

    uint16_t percentWidth, percentHeight;
    // Just pick a number for measurement. We don't want the UI moving when the number changes
    display.getTextSize("50%", &percentWidth, &percentHeight);

    int16_t barWidth = 60;
    x = (display.width() - barWidth - percentWidth - spacing) / 2;
    display.drawRect(x, y, barWidth, barHeight, display.fg);
    int16_t progressWidth = progress * barWidth / 100;
    display.fillRect(x, y, progressWidth, barHeight, display.fg);

    x += barWidth + spacing;
    display.drawSingleLineOfText(percentStr, x, y + display.getTextHeight() - 16);

    // Cursor is from the baseline of the text I think
    y += barHeight + display.getTextHeight();
    uint16_t textWidth, textHeight;
    auto message1 = "Press right and left";
    display.getTextSize(message1, &textWidth, &textHeight);
    display.drawSingleLineOfText(message1, (display.width() - textWidth) / 2, y);

    y += display.getTextHeight();
    auto message2 = "to seek through";
    display.getTextSize(message2, &textWidth, &textHeight);
    display.drawSingleLineOfText(message2, (display.width() - textWidth) / 2, y);

    y += display.getTextHeight();
    auto message3 = "book or article";
    display.getTextSize(message3, &textWidth, &textHeight);
    display.drawSingleLineOfText(message3, (display.width() - textWidth) / 2, y);

    y += display.getTextHeight() + 10;
    std::string message4 = "Seek granularity: ";
    message4 += StringFormat("%.1f%%", (float)(100 / (float)seekGranularity_));
    display.getTextSize(message4.c_str(), &textWidth, &textHeight);
    display.drawSingleLineOfText(message4.c_str(), (display.width() - textWidth) / 2, y);

    // up arrow
    uint16_t arrowWidth = 10, arrowHeight = 5;
    x = (display.width() - arrowWidth) / 2;
    y = 10;
    display.drawLine(x, y, x + arrowWidth / 2, y - arrowHeight, display.fg);
    display.drawLine(x + arrowWidth / 2, y - arrowHeight, x + arrowWidth, y, display.fg);

    // Down Arrow
    y = display.height() - 10;
    display.drawLine(x, y - arrowHeight, x + arrowWidth / 2, y, display.fg);
    display.drawLine(x + arrowWidth / 2, y, x + arrowWidth, y - arrowHeight, display.fg);

    display.updateDisplay(firstLoad_ ? DisplaySystem::UpdateType::FULL
                                     : DisplaySystem::UpdateType::FAST);
    firstLoad_ = false;
}

void ReadableSeekScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT: {
        // Do the -5% thing. This is unlikley to be right long-term
        // but it's way better if you overshoot.
        // TODO: Do we need placePrev anymore except for logging?
        readable_->placePrev = readable_->place;

        size_t lengthOfFile = renderer_.size();

        // Subtract 5% to the file
        if (readable_->place->getOffset() >= (lengthOfFile / seekGranularity_)) {
            readable_->place->setOffset(readable_->place->getOffset() -
                                        lengthOfFile / seekGranularity_);
        } else {
            readable_->place->setOffset(0);
        }

        log_d("Setting place from %s to %s", readable_->placePrev->str().c_str(),
              readable_->place->str().c_str());

        renderer_.setCurrentPlace(readable_->place);
        prefs_.readablePlace.set(readable_->shortenedUuid.c_str(), readable_->place->str());
        prefs_.readablePlaceChanged.set(readable_->shortenedUuid.c_str(), true);
        prefs_.lastReadTime.set(readable_->shortenedUuid.c_str(),
                                SolClock::getInstance().getEpochTime());
        setNeedsRerender();
        break;
    }
    case RemoteCommand::RIGHT: {
        // TODO: Do we need placePrev anymore except for logging?
        readable_->placePrev = readable_->place;

        size_t lengthOfFile = renderer_.size();

        // Add 5% to the file
        readable_->place->setOffset(readable_->place->getOffset() +
                                    (lengthOfFile / seekGranularity_));

        log_d("Setting place from %s to %s", readable_->placePrev->str().c_str(),
              readable_->place->str().c_str());

        // Don't set place past EOF
        if (readable_->place->getOffset() >= lengthOfFile) {
            readable_->place->setOffset(lengthOfFile - 1);
        }

        renderer_.setCurrentPlace(readable_->place);
        prefs_.readablePlace.set(readable_->shortenedUuid.c_str(), readable_->place->str());
        prefs_.readablePlaceChanged.set(readable_->shortenedUuid.c_str(), true);
        prefs_.lastReadTime.set(readable_->shortenedUuid.c_str(),
                                SolClock::getInstance().getEpochTime());
        setNeedsRerender();
        break;
    }
    case RemoteCommand::UP: {
        back();
        break;
    }
    case RemoteCommand::DOWN: {
        switch (seekGranularity_) {
        case 20:
            seekGranularity_ = 100;
            break;
        case 100:
            seekGranularity_ = 1000;
            break;
        case 1000:
            seekGranularity_ = 10;
            break;
        case 10:
            seekGranularity_ = 20;
        default:
            break;
        }
        setNeedsRerender();
        firstLoad_ = true;
        break;
    }
    case RemoteCommand::BACK: {
        back();
        break;
    }
    default:
        break;
    }
}
