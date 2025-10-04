#include "UI/DemoScreen.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "Communications/WiFi.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "UI/Bitmaps/mani-astronaut.hpp"
#include "UI/Bitmaps/mani-book1.hpp"
#include "UI/Bitmaps/mani-book2.hpp"
#include "UI/Bitmaps/mani-book3.hpp"
#include "UI/Bitmaps/mani-eye1.hpp"
#include "UI/Bitmaps/mani-eye2.hpp"
#include "UI/Bitmaps/mani-eye3.hpp"
#include "UI/Bitmaps/mani-eye4.hpp"
#include "UI/Bitmaps/mani-eye5.hpp"
#include "UI/Bitmaps/mani-eye6.hpp"
#include "UI/Bitmaps/mani-eye7.hpp"
#include "UI/Bitmaps/mani-eye8.hpp"
#include "UI/Bitmaps/mani-eye9.hpp"
#include "UI/Bitmaps/mani-phone1.hpp"
#include "UI/Bitmaps/mani-phone2.hpp"
#include "UI/Bitmaps/mani-phone3.hpp"
#include "UI/Bitmaps/mani-phone4.hpp"
#include "UI/Bitmaps/mani-phone5.hpp"
#include "UI/Bitmaps/mani-ship.hpp"
#include "UI/Bitmaps/mani-spark1.hpp"
#include "UI/Bitmaps/mani-spark2.hpp"
#include "UI/Bitmaps/mani-spark3.hpp"
#include "UI/Bitmaps/mani-spark4.hpp"
#include "UI/Bitmaps/mani-spark5.hpp"
#include "UI/Bitmaps/mani-turnpage-large-a.hpp"
#include "UI/Bitmaps/mani-turnpage-large-b.hpp"
#include "UI/Bitmaps/mani-turnpage-small.hpp"
#include "UI/Bitmaps/mani-volcano.hpp"
#include "UI/Bitmaps/mani-warrior.hpp"
#include "UI/Bitmaps/sol-logo.hpp"
#include "UI/Fonts.hpp"
#include "UI/Layout/Types.hpp"
#include "sindarin-debug.h"

DemoScreen::DemoScreen(DisplaySystem &display, PreferencesStore &prefs, bool fullManifesto)
    : display_(display),
      renderer_(display, GetTextRectFromPrefs(display, prefs), static_cast<Font *>(&fontFace0)),
      prefs_(prefs), originalInversion_(display_.getInversion()), fullManifesto_(fullManifesto) {
    // We want to control exactly when full and partial will happen.
    // This allows us to cross ownership boundaries and to force a partial update
    // even when we switch to the main menu the first time.
    display.forceDontChangeRefreshType();

    // Always run the manifesto in light mode.

    display_.invertDisplay(false);
    if (fullManifesto_) {
        lambdas_.emplace_back([this](DisplaySystem &display) {
            dropVcom_ = true;
            display.drawXBitmap((display.width() - mani_book1_width) / 2,
                                (display.height() - mani_book1_height) / 2, MANI_BOOK1_BITS,
                                mani_book1_width, mani_book1_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 1250;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_book2_width) / 2,
                                (display.height() - mani_book2_height) / 2, MANI_BOOK2_BITS,
                                mani_book2_width, mani_book2_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
            nextInterval_ = 1000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_book3_width) / 2,
                                (display.height() - mani_book3_height) / 2, MANI_BOOK3_BITS,
                                mani_book3_width, mani_book3_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
            nextInterval_ = 1000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_book3_width) / 2,
                                (display.height() - mani_book3_height) / 2, MANI_BOOK3_BITS,
                                mani_book3_width, mani_book3_height, display.bg);
            nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
            nextInterval_ = 1000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            dropVcom_ = false;
            display.clearDisplay();
            drawDoubleLine(display, "The way we read  ", "is always changing.");
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 2000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            drawDoubleLine(display, "But across the centuries", "one thing hasn't.");
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 2000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            drawDoubleLine(display, "What reading does", "for our minds.");
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 2000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            drawDoubleLine(display, "Whisking us away", "into other realms.");
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 1000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_turnpage_small_width) / 2,
                                (display.height() - mani_turnpage_small_height) / 2,
                                MANI_TURNPAGE_SMALL_BITS, mani_turnpage_small_width,
                                mani_turnpage_small_height, display.fg);
            drawDoubleLine(display, "Whisking us away", "into other realms.");
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
            nextInterval_ = 750;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap(display.width() - mani_turnpage_large_a_width,
                                display.height() - mani_turnpage_large_a_height,
                                MANI_TURNPAGE_LARGE_A_BITS, mani_turnpage_large_a_width,
                                mani_turnpage_large_a_height, display.fg);
            display.drawXBitmap(display.width() - mani_turnpage_large_b_width,
                                display.height() - mani_turnpage_large_a_height -
                                    mani_turnpage_large_b_height,
                                MANI_TURNPAGE_LARGE_B_BITS, mani_turnpage_large_b_width,
                                mani_turnpage_large_b_height, display.fg);
            drawDoubleLine(display, "Whisking us away", "into other realms.");
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
            nextInterval_ = 750;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            dropVcom_ = true;
            display.clearDisplay();
            display.drawXBitmap((display.width() - mani_volcano_width) / 2,
                                (display.height() - mani_volcano_height) / 2, MANI_VOLCANO_BITS,
                                mani_volcano_width, mani_volcano_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
            nextInterval_ = 2500;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_astronaut_width) / 2,
                                (display.height() - mani_astronaut_height) / 2, MANI_ASTRONAUT_BITS,
                                mani_astronaut_width, mani_astronaut_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::FULL;
            nextInterval_ = 2000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_ship_width) / 2,
                                (display.height() - mani_ship_height) / 2, MANI_SHIP_BITS,
                                mani_ship_width, mani_ship_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::FULL;
            nextInterval_ = 2000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_warrior_width) / 2,
                                (display.height() - mani_warrior_height) / 2, MANI_WARRIOR_BITS,
                                mani_warrior_width, mani_warrior_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::FULL;
            nextInterval_ = 2250;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            dropVcom_ = false;
            drawDoubleLine(display, "Sparking new", "thoughts and ideas.");
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::FULL;
            nextInterval_ = 2000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            dropVcom_ = true;
            display.clearDisplay();
            display.drawXBitmap((display.width() - mani_spark1_width) / 2,
                                (display.height() - mani_spark1_height) / 2, MANI_SPARK1_BITS,
                                mani_spark1_width, mani_spark1_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::FULL;
            nextInterval_ = 1500;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_spark2_width) / 2,
                                (display.height() - mani_spark2_height) / 2, MANI_SPARK2_BITS,
                                mani_spark2_width, mani_spark2_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 750;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_spark3_width) / 2,
                                (display.height() - mani_spark3_height) / 2, MANI_SPARK3_BITS,
                                mani_spark3_width, mani_spark3_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 750;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_spark4_width) / 2,
                                (display.height() - mani_spark4_height) / 2, MANI_SPARK4_BITS,
                                mani_spark4_width, mani_spark4_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 750;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_spark5_width) / 2,
                                (display.height() - mani_spark5_height) / 2, MANI_SPARK5_BITS,
                                mani_spark5_width, mani_spark5_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 1000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            dropVcom_ = false;
            display.invertDisplay(true);
            drawTripleLine(display, "In this era of", "constant beeps,", "alerts, and updates...");
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 2000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            dropVcom_ = true;
            display.invertDisplay(false);
            display.clearDisplay();
            display.drawXBitmap((display.width() - mani_phone1_width) / 2,
                                (display.height() - mani_phone1_height) / 2, MANI_PHONE1_BITS,
                                mani_phone1_width, mani_phone1_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 1000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_phone2_width) / 2,
                                (display.height() - mani_phone2_height) / 2, MANI_PHONE2_BITS,
                                mani_phone2_width, mani_phone2_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 750;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_phone3_width) / 2,
                                (display.height() - mani_phone3_height) / 2, MANI_PHONE3_BITS,
                                mani_phone3_width, mani_phone3_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 750;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_phone4_width) / 2,
                                (display.height() - mani_phone4_height) / 2, MANI_PHONE4_BITS,
                                mani_phone4_width, mani_phone4_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 750;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            display.drawXBitmap((display.width() - mani_phone5_width) / 2,
                                (display.height() - mani_phone5_height) / 2, MANI_PHONE5_BITS,
                                mani_phone5_width, mani_phone5_height, display.fg);
            nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
            nextInterval_ = 1000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            dropVcom_ = false;
            display.invertDisplay(true);
            drawTripleLine(display, "...what will the", "next evolution of", "reading look like?");
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::FULL;
            nextInterval_ = 2250;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            drawTripleLine(display, "Simply lean back,", "open your eyes, and",
                           "let words light the way.");
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::FULL;
            nextInterval_ = 3000;
        });

        lambdas_.emplace_back([this](DisplaySystem &display) {
            drawTripleLine(display, "Simply lean back,", "open your eyes, and",
                           "let words light the way.", true);
            drawSkipMessage(display);
            nextUpdateType_ = DisplaySystem::UpdateType::ERASE;
            nextInterval_ = 10;
        });
        // endif of fullManifesto_
    }

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = true;
        display.invertDisplay(false);
        display.drawXBitmap((display.width() - mani_eye1_width) / 2,
                            (display.height() - mani_eye1_height) / 2, MANI_EYE1_BITS,
                            mani_eye1_width, mani_eye1_height, display.fg);
        std::string message;
        if (!fullManifesto_) {
            message = "Press \ue000 to begin";
            awaitingButton_ = true;
        } else {
            message = "Welcome to Sol!";
            nextInterval_ = 2000;
        }
        display.drawSingleLineOfText(message, (display.width() - display.getTextWidth(message)) / 2,
                                     (display.height() - 256) / 2 + 202, true);

        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.invertDisplay(false);
        display.drawXBitmap((display.width() - mani_eye2_width) / 2,
                            (display.height() - mani_eye2_height) / 2, MANI_EYE2_BITS,
                            mani_eye2_width, mani_eye2_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 1000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap((display.width() - mani_eye3_width) / 2,
                            (display.height() - mani_eye3_height) / 2, MANI_EYE3_BITS,
                            mani_eye3_width, mani_eye3_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 800;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap((display.width() - mani_eye4_width) / 2,
                            (display.height() - mani_eye4_height) / 2, MANI_EYE4_BITS,
                            mani_eye4_width, mani_eye4_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 600;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap((display.width() - mani_eye5_width) / 2,
                            (display.height() - mani_eye5_height) / 2, MANI_EYE5_BITS,
                            mani_eye5_width, mani_eye5_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap((display.width() - mani_eye6_width) / 2,
                            (display.height() - mani_eye6_height) / 2, MANI_EYE6_BITS,
                            mani_eye6_width, mani_eye6_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap((display.width() - mani_eye7_width) / 2,
                            (display.height() - mani_eye7_height) / 2, MANI_EYE7_BITS,
                            mani_eye7_width, mani_eye7_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap((display.width() - mani_eye8_width) / 2,
                            (display.height() - mani_eye8_height) / 2, MANI_EYE8_BITS,
                            mani_eye8_width, mani_eye8_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 500;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        display.drawXBitmap((display.width() - mani_eye9_width) / 2,
                            (display.height() - mani_eye9_height) / 2, MANI_EYE9_BITS,
                            mani_eye9_width, mani_eye9_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::PARTIAL;
        nextInterval_ = 2000;
    });

    lambdas_.emplace_back([this](DisplaySystem &display) {
        dropVcom_ = true;
        display.invertDisplay(true);
        display.drawXBitmap((display.width() - sol_logo_width) / 2,
                            (display.height() - sol_logo_height) / 2, SOL_LOGO_BITS, sol_logo_width,
                            sol_logo_height, display.fg);
        nextUpdateType_ = DisplaySystem::UpdateType::FULL;
        nextInterval_ = 2500;
    });
}

void DemoScreen::drawSkipMessage(DisplaySystem &display, bool inverted) {
    std::string message = "Press \ue000 to skip";
    display.drawSingleLineOfText(message, (display.width() - display.getTextWidth(message)) / 2,
                                 235, inverted);
}

void DemoScreen::drawDoubleLine(DisplaySystem &display, std::string one, std::string two) {
    std::string message;
    Font *font = static_cast<Font *>(&fontFace1);
    message = std::move(one);
    display.drawSingleLineOfText(
        message, (display.width() - display.getTextWidth(message, font)) / 2,
        (display.height() - display.getTextHeight(message, font)) / 2 - 12, false, font);
    message = std::move(two);
    display.drawSingleLineOfText(
        message, (display.width() - display.getTextWidth(message, font)) / 2,
        (display.height() + display.getTextHeight(message, font)) / 2 - 4, false, font);
}

void DemoScreen::drawTripleLine(DisplaySystem &display, std::string one, std::string two,
                                std::string three, bool invert) {
    std::string message;
    Font *font = static_cast<Font *>(&fontFace1);
    message = std::move(one);
    int yAdjust = font->lineHeight();

    yAdjust = (yAdjust + 1) / 2;

    display.drawSingleLineOfText(message,
                                 (display.width() - display.getTextWidth(message, font)) / 2,
                                 display.height() / 2 - yAdjust - yAdjust * 2 - 4, invert, font);
    message = std::move(two);
    display.drawSingleLineOfText(message,
                                 (display.width() - display.getTextWidth(message, font)) / 2,
                                 display.height() / 2 - yAdjust, invert, font);
    message = std::move(three);
    display.drawSingleLineOfText(message,
                                 (display.width() - display.getTextWidth(message, font)) / 2,
                                 display.height() / 2 + yAdjust + 4, invert, font);
}

void DemoScreen::finishDemo() {
    // When we're done with the onboarding screens, turn this mode off.
    dropVcom_ = false;
    display_.invertDisplay(originalInversion_);

    back();
}

void DemoScreen::render(DisplaySystem &display) {
    if (currentScreen_ >= lambdas_.size()) {
        finishDemo();
        return;
    }
    lambdas_[currentScreen_](display);

    // Must stop the timer because ChangePeriod starts it.
    xTimerStopFromISR(DisplaySystem::animationTimer, 0);

    if (!awaitingButton_) {
        // Move to the next animation
        xTimerChangePeriod(DisplaySystem::animationTimer, pdMS_TO_TICKS(nextInterval_), 0);
    }
    int nextVcomNudge = 0;
    if (dropVcom_) {
        nextVcomNudge = -2500;
    } else {
        nextVcomNudge = 0;
    }
    display.updateDisplay(nextUpdateType_, true, nextVcomNudge);
    if (!awaitingButton_) {
        currentScreen_++;
    }
}

void DemoScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::ENTER:
        if (awaitingButton_) {
            awaitingButton_ = false;
            xTimerStart(DisplaySystem::animationTimer, 0);
            currentScreen_++;
            setNeedsRerender();
        } else {
            xTimerStop(DisplaySystem::animationTimer, 0);
            // Gives us the full refresh
            display_.clearDontChangeRefreshType();
            display_.clearDisplay();
            display_.clearFrameBuffer();
            finishDemo();
        }
        break;
    case RemoteCommand::ANIMATION:
        setNeedsRerender();
        break;
    case RemoteCommand::UP:
    case RemoteCommand::RIGHT:
    case RemoteCommand::DOWN:
    case RemoteCommand::BACK:
    default:
        break;
    }
}
