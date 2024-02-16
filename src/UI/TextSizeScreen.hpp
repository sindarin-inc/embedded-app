#pragma once

#include "Misc/Battery.hpp"
#include "Misc/Telemetry.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/Screen.hpp"

// Eventually we'll want to refactor this with the OffsetAdjustScreen
// since so much of the functionality is the same
// This is just to test that this concept works ok.
class TextSizeScreen : public MenuScreen {
public:
    TextSizeScreen(Battery &battery, DisplaySystem &display, MenuStyleFamily myStyle,
                   PreferencesStore &prefs, Telemetry &telemetry, std::string menuName = "Default",
                   std::string upperLeftText = "Main Menu")
        : MenuScreen(battery, display, myStyle, std::move(menuName), std::move(upperLeftText)),
          display_(display), prefs_(prefs), telemetry_(telemetry) {
        setExtraText("Example text size");
        addMenuItem(MenuItem("Small", [this]() {
            this->display_.style.setFontFamily(this->display_.style.solSansSmall);
            this->display_.clearDisplay();
            this->prefs_.font.set(PREFS_SOL_SMALL);
            this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE, {{"text_size", "small"}});
            setNeedsRerender();
        }));
        addMenuItem(MenuItem("Medium", [this]() {
            this->display_.style.setFontFamily(this->display_.style.solSansMedium);
            this->display_.clearDisplay();
            this->prefs_.font.set(PREFS_SOL_MEDIUM);
            this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE, {{"text_size", "medium"}});
            setNeedsRerender();
        }));
        addMenuItem(MenuItem("Large", [this]() {
            this->display_.style.setFontFamily(this->display_.style.solSansLarge);
            this->display_.clearDisplay();
            this->prefs_.font.set(PREFS_SOL_LARGE);
            this->telemetry_.logMetricData(TelemetryField::METRIC_STYLE, {{"text_size", "large"}});
            setNeedsRerender();
        }));
        int menuIndex = 0;
        if (prefs.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_SMALL) {
            menuIndex = 0;
        } else if (prefs.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_MEDIUM) {
            menuIndex = 1;
        } else if (prefs.font.get().value_or(PREFS_SOL_SMALL) == PREFS_SOL_LARGE) {
            menuIndex = 2;
        }
        setCurrSelection(menuIndex);
    };

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
};
