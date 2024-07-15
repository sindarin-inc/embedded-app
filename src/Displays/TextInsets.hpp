// Utility methods for working with saved text insets for the text renderer

#include "Displays/DisplaySystem.hpp"
#include "Storage/PreferencesStore.hpp"

auto GetTextInsetFromPrefs(DisplaySystem &display, PreferencesStore &prefs) -> DisplayInset;

auto GetTextRectFromPrefs(DisplaySystem &display, PreferencesStore &prefs) -> DisplayRect;

auto SetTextInset(PreferencesStore &prefs, DisplayInset insets) -> void;
