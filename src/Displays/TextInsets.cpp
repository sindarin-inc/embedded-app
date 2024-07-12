#include "TextInsets.hpp"

auto GetTextInsetFromPrefs(DisplaySystem &display, PreferencesStore &prefs) -> DisplayInset {
    return {prefs.textInsetLeft.get().value_or(display.defaultTextInsets().left),
            prefs.textInsetRight.get().value_or(display.defaultTextInsets().right),
            prefs.textInsetTop.get().value_or(display.defaultTextInsets().top),
            prefs.textInsetBottom.get().value_or(display.defaultTextInsets().bottom)};
}

auto GetTextRectFromPrefs(DisplaySystem &display, PreferencesStore &prefs) -> DisplayRect {
    DisplayInset insets = GetTextInsetFromPrefs(display, prefs);
    return DisplayRect::fromSizeAndInsets(display.width(), display.height(), insets);
}

auto SetTextInset(PreferencesStore &prefs, DisplayInset insets) -> void {
    prefs.textInsetLeft.set(insets.left);
    prefs.textInsetRight.set(insets.right);
    prefs.textInsetTop.set(insets.top);
    prefs.textInsetBottom.set(insets.bottom);
}
