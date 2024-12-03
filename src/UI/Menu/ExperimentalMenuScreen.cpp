#include "ExperimentalMenuScreen.hpp"

#include "Misc/Platform.hpp"
#include "UI/Fonts/Fonts.hpp"

ExperimentalMenuScreen::ExperimentalMenuScreen(DisplaySystem &display, const std::string &backTitle,
                                               PreferencesStore &prefs, Telemetry &telemetry,
                                               PushScreenFunc pushScreen)
    : EinkView(display), menuView_(display), backLabelView_(display, backTitle),
      renderer_(display, DisplayRect({0, 0, 0, 0}), static_cast<Font *>(&fontFace0)), prefs_(prefs),
      telemetry_(telemetry), pushScreen_(std::move(pushScreen)) {
    menuView_.padding = 1;
    menuView_.itemsPerPage = 8;
}

void ExperimentalMenuScreen::addMenuItem(const std::shared_ptr<MenuItem> &menuItem) {
    menuView_.addMenuItem(menuItem);
}

void ExperimentalMenuScreen::resetFont() {
    Font *font = FontFromPrefs(prefs_.font.get().value_or(PREFS_SOL_SMALL));

    if (font) {
        renderer_.setFont(font);
    }
}

void ExperimentalMenuScreen::processCommand(RemoteCommand command) {
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
        back();
        return;
    case RemoteCommand::RIGHT:
        menuView_.executeSelectedItemRightButton();
        return;
    default:
        // Handle other commands or ignore
        break;
    }
    setNeedsLayout();
}

auto ExperimentalMenuScreen::setSelectedIndex(size_t index) -> bool {
    auto newPage = menuView_.setSelectedIndex(index);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
    return newPage;
}

auto ExperimentalMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    int padding = size.height / 20;

    // Layout back label
    context.setViewFrame(backLabelView_, {{0, 0}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignLeft);
    // Layout menu
    context.setViewFrame(menuView_, Rect::fromSize(size).inset(padding),
                         LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    return size;
}

void ExperimentalMenuScreen::drawInRect(Rect rect) {
    // Always load the font from prefs
    resetFont();
    backLabelView_.draw();
    menuView_.draw();
}

void ExperimentalMenuScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

void ExperimentalMenuScreen::render(DisplaySystem &display) {
    setFrame(defaultFrameRect());
    draw();
    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }
    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}

void ExperimentalMenuScreen::populateMenuItems() {

    addMenuItem(std::make_shared<MenuItemCheckable>(
        "Show Partial Summary",
        [this]() {
        this->prefs_.noShowPartialChapterSummary.set(
            !this->prefs_.noShowPartialChapterSummary.get().value_or(false));
        setNeedsRerender();
        if (!prefs_.seenPartialSummaryOverview.get().value_or(false)) {
            std::string message =
                "You know how when you binge watch a show (you'd never do that, would "
                "you?), you often will get a \"Previously on\" overview "
                "at the beginning of each episode? This feature gives you \"Previously "
                "on\" "
                "but for your reading! Any time you leave a reading "
                "session in the middle of a chapter, we'll generate a partial chapter "
                "summary for you (no spoilers) to "
                "review on your next reading session.\n\n\n"
                "Press any button to continue...";
            RootUI::getInstance().showTemporaryMessage(message, TEMP_MSG_DURATION_NO_CLOSE);
            prefs_.seenPartialSummaryOverview.set(true);
        }
        },
        [this]() -> bool {
            return !this->prefs_.noShowPartialChapterSummary.get().value_or(false);
        }));

    addMenuItem(std::make_shared<MenuItemCheckable>(
        "Fast Page Transitions",
        [this]() {
        this->prefs_.fastTransitions.set(!this->prefs_.fastTransitions.get().value_or(false));
        setNeedsRerender();
        if (!prefs_.seenFastPageTransitionsOverview.get().value_or(false)) {
            std::string message =
                "Have you noticed on some screen transitions, the Eink "
                "displays of your Sol Reader flash black and then white? "
                "This is called a full refresh. By default, all page "
                "turns use full refresh. "
                "The Fast Page Transitions "
                "option speeds up your page turns "
                "by only doing a full refresh when needed to ensure the display doesn't "
                "have too many remnants of previous pages.\n\n"
                "Press any button to continue...";
            RootUI::getInstance().showTemporaryMessage(message, TEMP_MSG_DURATION_NO_CLOSE);
            prefs_.seenFastPageTransitionsOverview.set(true);
        }
        },
        [this]() -> bool { return this->prefs_.fastTransitions.get().value_or(false); }));

    // Helper function to convert 24h to 12h format with AM/PM
    auto formatTime = [](int hour) -> std::string {
        if (hour == -1) {
            return "Manual";
        }
        const char *period = (hour >= 12) ? "pm" : "am";
        int displayHour = (hour == 0) ? 12 : ((hour > 12) ? hour - 12 : hour);
        return StringFormat("%d%s", displayHour, period);
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

    auto toggleDarkMode = [this]() {
        // Toggle dark mode
        this->prefs_.darkMode.set(!this->prefs_.darkMode.get().value_or(false));
        telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                 {{"dark_mode", this->prefs_.darkMode.get().value_or(false)}});
        display_.flipInversion();
        display_.clearDisplay();
        prefs_.invertSetting.set(display_.getInversion());
        setNeedsRerender();
        setFirstLoad();
    };

    auto isDarkModeChecked = [this]() -> bool {
        return this->prefs_.darkMode.get().value_or(false);
    };

    auto centerCallback =
        [this, formatTime, toggleDarkMode]() { // Add toggleDarkMode to capture list
        auto darkModeMenu = std::make_shared<GenericSettingsMenuScreen>(display_, "Reading Style");

        darkModeMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
            StringFormat("Turn on after: %s",
                         formatTime(prefs_.invertStartHour.get().value_or(-1)).c_str()),
            [this, formatTime]() {
            auto timeMenu = std::make_shared<GenericSettingsMenuScreen>(display_, "Dark Mode");

            timeMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Manual", [this]() {
                prefs_.invertStartHour.set(-1); // Only clear start hour
                telemetry_.logMetricData(TelemetryField::METRIC_STYLE, {{"dark_mode_start", -1}});
                back();
                back();
                regenerateMenuItems();
                setNeedsRerender();
            }));

            for (int hour = 0; hour < 24; hour++) {
                timeMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(formatTime(hour),
                                                                           [this, hour]() {
                    prefs_.invertStartHour.set(hour);
                    telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                             {{"dark_mode_start", hour}});
                    back();
                    back();
                    regenerateMenuItems();
                    setNeedsRerender();
                }));
            }

            int menuIndex = 0;
            if (prefs_.invertStartHour.get().has_value()) {
                menuIndex = prefs_.invertStartHour.get().value() + 1;
            } else {
                menuIndex = 21; // Start at 8pm
            }
            timeMenu->setSelectedIndex(menuIndex);

            timeMenu->setBackCallback([this]() { back(); });
            pushScreen_(timeMenu);
            }));

        darkModeMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
            StringFormat("Turn off after: %s",
                         formatTime(prefs_.invertEndHour.get().value_or(-1)).c_str()),
            [this, formatTime]() {
            auto timeMenu = std::make_shared<GenericSettingsMenuScreen>(display_, "Dark Mode");

            timeMenu->addMenuItem(std::make_shared<MenuItemSingleLine>("Manual", [this]() {
                prefs_.invertEndHour.set(-1); // Only clear end hour
                telemetry_.logMetricData(TelemetryField::METRIC_STYLE, {{"dark_mode_end", -1}});
                back();
                back();
                regenerateMenuItems();
                setNeedsRerender();
            }));

            for (int hour = 0; hour < 24; hour++) {
                timeMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(formatTime(hour),
                                                                           [this, hour]() {
                    prefs_.invertEndHour.set(hour);
                    telemetry_.logMetricData(TelemetryField::METRIC_STYLE,
                                             {{"dark_mode_end", hour}});
                    back();
                    back();
                    regenerateMenuItems();
                    setNeedsRerender();
                }));
            }

            int menuIndex = 0;
            if (prefs_.invertEndHour.get().has_value()) {
                menuIndex = prefs_.invertEndHour.get().value() + 1;
            } else {
                menuIndex = 9; // End at 8am
            }
            timeMenu->setSelectedIndex(menuIndex);

            timeMenu->setBackCallback([this]() { back(); });
            pushScreen_(timeMenu);
            }));

        // Add new menu items to manually toggle dark mode
        darkModeMenu->addMenuItem(std::make_shared<MenuItemSingleLine>(
            prefs_.darkMode.get().value_or(false) ? "Turn dark mode off" : "Turn dark mode on",
            [this, toggleDarkMode]() { // Explicitly capture centerCallback here too
            toggleDarkMode();          // Reuse the existing dark mode toggle code
            back();
            regenerateMenuItems();
            setNeedsRerender();
            }));

        darkModeMenu->setBackCallback([this]() {
            back();
            if (prefs_.invertStartHour.get().value_or(-1) != -1) {
                // TODO: Add logic to check current time and set dark mode accordingly
            }
        });

        pushScreen_(darkModeMenu);

        if (!prefs_.seenAutoDarkModeOverview.get().value_or(false)) {
            std::string message =
                "Would you like your Sol Reader to automatically be in Dark Mode before "
                "sunrise and after sunset? If so, this feature is for you! "
                "You can set the time for Dark Mode to automatically turn on and off with this "
                "Auto Dark Mode feature. Need to manaully override? You can do that here too!\n\n"
                "Press any button to continue...";
            RootUI::getInstance().showTemporaryMessage(message, TEMP_MSG_DURATION_NO_CLOSE);
            prefs_.seenAutoDarkModeOverview.set(true);
        }
    };

    addMenuItem(std::make_shared<DarkModeMenuItemCheckable>(
        StringFormat("Auto Dark Mode%s",
                     getDarkModeScheduleText(prefs_.invertStartHour.get().value_or(-1),
                                             prefs_.invertEndHour.get().value_or(-1), formatTime)
                         .c_str()),
        centerCallback, isDarkModeChecked));
}
