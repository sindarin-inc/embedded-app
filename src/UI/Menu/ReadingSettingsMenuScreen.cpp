#include "ReadingSettingsMenuScreen.hpp"

#include "Misc/Platform.hpp"
#include "UI/Fonts/Fonts.hpp"

ReadingSettingsMenuScreen::ReadingSettingsMenuScreen(DisplaySystem &display,
                                                     const std::string &backTitle,
                                                     PreferencesStore &prefs, Telemetry &telemetry,
                                                     PushScreenFunc pushScreen,
                                                     PopScreenFunc popScreen)
    : EinkView(display), menuView_(display), backLabelView_(display, backTitle),
      renderer_(display, DisplayRect({0, 0, 0, 0}), static_cast<Font *>(&fontFace0)),
      rendererView_(display, renderer_), prefs_(prefs), telemetry_(telemetry),
      pushScreen_(std::move(pushScreen)), popScreen_(std::move(popScreen)) {
    menuView_.padding = 1;
    menuView_.itemsPerPage = 8;
    rendererView_.dottedBox = true;
    rendererView_.setAutoPilotWraparound();

    std::string message;
    message += R"(The Expanse of the Lord of the Rings

    by James S.J.R.R.A. Tolkien Christensen


    "Elen síla lúmenn' omentielvo" Frodo said to Captain James Holden. (A star shines on the hour of our meeting).
    
    "Yes, Frodo." Holden answered. "There are about 1300 stars we could visit whenever you're up for it. But please get rid of that evil jewelry. We wouldn't want the protomolecule to get a hold of that."

    "Yeah, about that..." Frodo said with a frown. "Gandalf wouldn't get those giant eagles involved. Said they were too high key or something. But perhaps they're not high key enough? Perhaps these Rail Guns and Point Defense Cannons of yours work better on orcs and wraiths than the flying spies of Manwe?

    "Sure, I suppose this is essentially an engineering and a tactical conundrum" Holden said considering the proposed mission - "Getting the device to this special volcano past a supernatural despot and his army of zombies. It's not thatdifferent from missions we've had before. At least there were zombies. I'll ask Naomi to look at the details when we're back in her orbit around Eärendil or whatever you called Venus around here..."

    Frodo didn't really know where to begin a response to Holden's peculiar monologue but before he could answer, Amos chimed in with his usual grin and the words "Is there money involved in this? Might come in handy."

    "Well, my uncle Bilbo is sort of wealthy and left it all to me when he disappeared at his birthday party. I also know a lady in an enchanted forest that makes some pretty useful stuff."
    
    "THREE STRANDS OF HER GOLDEN HAIR....1!1ONE11" Gimli exclaimed for the hundredth time, tears streaming happily down his braided beard.

    "Yeah, don't really know what we would do with that, Master Dwarf" Amos answered, a bit puzzled. "But your uncle's old wealth sounds perfectly fine, little man" Amos said to Frodo, polishing the shotgun he procured from his back.

    Whilst this conversation continued, Peregrin Took wandered off, curiosity leading him inside the Martian spaceship parked in the wilderness. Up the dark ladder, past the galley and finally into the company of Alex Kamal. The best pilot in the universe, apparently not interested in exploring Middle Earth instead of watching another movie on his personal terminal. 

    "Whatever you do don't push any buttons in here" Alex said in his Mariner Valley drawl somehow resembling the accent of Texas, which in the third age of this world still was called "Southern Rhûn".

    "Buttons? What buttons?" Pippin asked looking around for winter cloaks, absent-mindedly leaning on a particularly big red half dome on the weirdly twinkling table around Alex' swiveling couch...

    The usually dormant A.I. of the Rocinante suddenly awoke. Sensing the newfound power node wrapped in a little golden circle. "This technology seems preciousss to usss" it thought while firing up its Epstein drive frying the Fellowship straight to Valinor - or wherever mortals end up, beyond the circles of this world.

    And that my friends, is why Eru Ilúvatar invented time travel; with the words booming through the void...

    "FOOL OF A TOOK!"
    )";

    auto stream = std::make_shared<StdStringStream>(message);
    renderer_.setStream(stream);
}

void ReadingSettingsMenuScreen::addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void ReadingSettingsMenuScreen::resetFont() {
    Font *font = FontFromPrefs(prefs_.font.get().value_or(PREFS_SOL_SMALL));

    if (font) {
        renderer_.setFont(font);
    }
}

void ReadingSettingsMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        moveSelection(-1);
        break;
    case RemoteCommand::DOWN:
        moveSelection(1);
        break;
    case RemoteCommand::ENTER:
        menuView_.executeSelectedItem();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        firstLoad_ = true;
        setNeedsRerender();
        leaveMenu();      // Stop animation timer and reset renderer
        popScreen_(true); // Return to main menu
        return;
    case RemoteCommand::RIGHT:
        menuView_.executeSelectedItemSubmenu();
        return;
    // We rely on the animation timer which is set in the renderer view in autopilot mode
    case RemoteCommand::ANIMATION:
        setNeedsRerender();
        break;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

auto ReadingSettingsMenuScreen::setSelectedIndex(size_t index) -> bool {
    auto newPage = menuView_.setSelectedIndex(index);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
    return newPage;
}

auto ReadingSettingsMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int padding = size.height / 20;

    // Layout back label
    auto backBox = context.setViewFrame(backLabelView_, {{0, 0}, {size.width, 0}},
                                        LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);
    // Layout menu
    auto menuBox = context.setViewFrame(menuView_,
                                        Rect::fromSize(size).inset(size.width / 7, 0,
                                                                   backBox.size.height + padding,
                                                                   backBox.size.height + padding),
                                        LayoutOptionsSizeToFitVertical);

    int yOffset = menuBox.bottom() + padding;

    context.setViewFrame(rendererView_, {{0, yOffset}, {size.width, size.height - yOffset}});

    return size;
}

void ReadingSettingsMenuScreen::drawInRect(Rect rect) {
    // Always load the font from prefs
    resetFont();
    backLabelView_.draw();
    menuView_.draw();
    rendererView_.draw();
}

void ReadingSettingsMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

void ReadingSettingsMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    if (firstLoad_ || !rendererView_.autoPilotMode()) {
        draw();
        if (firstLoad_) {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
            firstLoad_ = false;
        }
        menuView_.drawFastElements();
        display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
    } else if (rendererView_.autoPilotMode()) {
        draw();
        menuView_.drawFastElements();
        if (rendererView_.newPage()) {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, true);
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FAST_ORIGINAL_AU4, true);
        }
    }
}

void ReadingSettingsMenuScreen::populateMenuItems() {
    addMenuItem(std::make_shared<NewMenuItemSingleLine>("Text Size", [this]() {
        auto fontMenu = std::make_shared<ReadingSettingsMenuScreen>(
            display_, "Reading Style", prefs_, telemetry_, pushScreen_, popScreen_);
        fontMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Small", [this]() {
            display_.clearDisplay();
            prefs_.font.set(PREFS_SOL_SMALL);
            setNeedsRerender();
        }));
        fontMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Medium", [this]() {
            display_.clearDisplay();
            prefs_.font.set(PREFS_SOL_MEDIUM);
            setNeedsRerender();
        }));
        fontMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Large", [this]() {
            display_.clearDisplay();
            prefs_.font.set(PREFS_SOL_LARGE);
            setNeedsRerender();
        }));
        int menuIndex = 0;
        if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_SMALL) {
            menuIndex = 0;
        } else if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_MEDIUM) {
            menuIndex = 1;
        } else if (prefs_.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_LARGE) {
            menuIndex = 2;
        }
        fontMenu->setSelectedIndex(menuIndex);
        pushScreen_(fontMenu);
        fontMenu->setBackCallback([this]() { popScreen_(false); });
    }));

    addMenuItem(std::make_shared<NewMenuItemSingleLine>("Move Page in Display", [this]() {
        log_d("Loading adjust offset");
        auto offsetAdjust = std::make_shared<OffsetAdjustScreen>(display_, prefs_, telemetry_);
        pushScreen_(offsetAdjust);
        log_d("setting up callback");
        offsetAdjust->setBackCallback([this]() { popScreen_(false); });
        log_d("set up callback");
    }));

    addMenuItem(std::make_shared<NewMenuItemSingleLine>("Resize Page", [this]() {
        log_d("Loading resize box");
        auto resizeBox = std::make_shared<ResizeBoxScreen>(display_, prefs_, telemetry_);
        pushScreen_(resizeBox);
        log_d("setting up callback");
        resizeBox->setBackCallback([this]() { popScreen_(false); });
        log_d("set up callback");
    }));

    addMenuItem(std::make_shared<NewMenuItemSingleLine>("Line Spacing", [this]() {
        auto lineSpacingScreen = std::make_shared<LineSpacingScreen>(display_, prefs_, telemetry_);
        pushScreen_(lineSpacingScreen);
        lineSpacingScreen->setBackCallback([this]() {
            popScreen_(false);
            setRendererLineSpacingFromPrefs();
        });
    }));

    // Helper function to convert 24h to 12h format with AM/PM
    auto formatTime = [](int hour) -> std::string {
        if (hour == -1) return "Manual";
        const char *period = (hour >= 12) ? "pm" : "am";
        int displayHour = (hour == 0) ? 12 : ((hour > 12) ? hour - 12 : hour);
        return StringFormat("%d:00%s", displayHour, period);
    };

    // Helper function to get the dark mode schedule text
    auto getDarkModeScheduleText = [](int startHour, int endHour,
                                      const auto &formatTime) -> std::string {
        if (startHour == -1 && endHour == -1) {
            return " (Manual)";
        } else if (startHour == -1) {
            return StringFormat(" (Off at %s)", formatTime(endHour).c_str());
        } else if (endHour == -1) {
            return StringFormat(" (On at %s)", formatTime(startHour).c_str());
        } else {
            return StringFormat(" (%s-%s)", formatTime(startHour).c_str(),
                                formatTime(endHour).c_str());
        }
    };

    auto centerCallback = [this]() {
        // Toggle dark mode
        this->prefs_.darkMode.set(!this->prefs_.darkMode.get().value_or(false));
        display_.flipInversion();
        display_.clearDisplay();
        prefs_.invertSetting.set(display_.getInversion());
        setNeedsRerender();
        setFirstLoad();
    };

    auto isDarkModeChecked = [this]() -> bool {
        return this->prefs_.darkMode.get().value_or(false);
    };

    auto rightCallback =
        [this, formatTime, centerCallback]() { // Add centerCallback to capture list
        auto darkModeMenu = std::make_shared<GenericSettingsMenuScreen>(display_, "Reading Style");

        darkModeMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
            StringFormat("Turn on after: %s",
                         formatTime(prefs_.invertStartHour.get().value_or(-1)).c_str()),
            [this, formatTime]() {
            auto timeMenu = std::make_shared<GenericSettingsMenuScreen>(display_, "Dark Mode");

            timeMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Manual", [this]() {
                prefs_.invertStartHour.set(-1); // Only clear start hour
                popScreen_(true);
                popScreen_(true);
                regenerateMenuItems();
                setNeedsRerender();
            }));

            for (int hour = 0; hour < 24; hour++) {
                timeMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(formatTime(hour),
                                                                              [this, hour]() {
                    prefs_.invertStartHour.set(hour);
                    popScreen_(true);
                    popScreen_(true);
                    regenerateMenuItems();
                    setNeedsRerender();
                }));
            }

            int menuIndex = 0;
            if (prefs_.invertStartHour.get().has_value()) {
                menuIndex = prefs_.invertStartHour.get().value() + 1;
            }
            timeMenu->setSelectedIndex(menuIndex);

            timeMenu->setBackCallback([this]() { popScreen_(true); });
            pushScreen_(timeMenu);
            }));

        darkModeMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
            StringFormat("Turn off after: %s",
                         formatTime(prefs_.invertEndHour.get().value_or(-1)).c_str()),
            [this, formatTime]() {
            auto timeMenu = std::make_shared<GenericSettingsMenuScreen>(display_, "Dark Mode");

            timeMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>("Manual", [this]() {
                prefs_.invertEndHour.set(-1); // Only clear end hour
                popScreen_(true);
                popScreen_(true);
                regenerateMenuItems();
                setNeedsRerender();
            }));

            for (int hour = 0; hour < 24; hour++) {
                timeMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(formatTime(hour),
                                                                              [this, hour]() {
                    prefs_.invertEndHour.set(hour);
                    popScreen_(true);
                    popScreen_(true);
                    regenerateMenuItems();
                    setNeedsRerender();
                }));
            }

            int menuIndex = 0;
            if (prefs_.invertEndHour.get().has_value()) {
                menuIndex = prefs_.invertEndHour.get().value() + 1;
            }
            timeMenu->setSelectedIndex(menuIndex);

            timeMenu->setBackCallback([this]() { popScreen_(true); });
            pushScreen_(timeMenu);
            }));

        // Add new menu items to manually toggle dark mode
        darkModeMenu->addMenuItem(std::make_shared<NewMenuItemSingleLine>(
            prefs_.darkMode.get().value_or(false) ? "Turn dark mode off" : "Turn dark mode on",
            [this, centerCallback]() { // Explicitly capture centerCallback here too
            centerCallback();          // Reuse the existing dark mode toggle code
            popScreen_(true);
            regenerateMenuItems();
            setNeedsRerender();
            }));

        darkModeMenu->setBackCallback([this]() {
            popScreen_(true);
            if (prefs_.invertStartHour.get().value_or(-1) != -1) {
                // TODO: Add logic to check current time and set dark mode accordingly
            }
        });

        pushScreen_(darkModeMenu);
    };

    addMenuItem(std::make_shared<DarkModeMenuItemCheckable>(
        StringFormat("Dark Mode%s",
                     getDarkModeScheduleText(prefs_.invertStartHour.get().value_or(-1),
                                             prefs_.invertEndHour.get().value_or(-1), formatTime)
                         .c_str()),
        centerCallback, isDarkModeChecked, rightCallback));

    addMenuItem(std::make_shared<NewMenuItemCheckable>(
        "Autopilot",
        [this]() {
        this->prefs_.autoPilot.set(!this->prefs_.autoPilot.get().value_or(false));
        if (this->prefs_.autoPilot.get().value_or(false)) {
            prefs_.autoPilotSpeed.set(10000);
            turnOnAutoPilot();
        } else {
            prefs_.autoPilotSpeed.remove();
            turnOffAutoPilot();
        }
        setNeedsRerender();
        setFirstLoad();
        },
        [this]() -> bool { return this->prefs_.autoPilot.get().value_or(false); }));
}
