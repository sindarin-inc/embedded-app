#pragma once

#include <Adafruit_GFX.h>

#include "UI/Fonts/Fonts.h"

struct FontFamily {
    Font *settingsFont;
    Font *mainMenuFont;
    Font *popupFont;
    Font *readableFont;
};

class Style {
public:
    Style() {
        lucidaRegular.mainMenuFont = (Font *)&FONT_LUCIDA_GRANDE10PT7B;
        lucidaRegular.settingsFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B;
        lucidaRegular.popupFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B;
        // lucidaRegular.readableFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B;
        lucidaRegular.readableFont = (Font *)&FONT_ECSANS_REGULAR_100BPI12PT;

        lucidaFat.mainMenuFont = (Font *)&FONT_LUCIDA_GRANDE10PT7B;
        lucidaFat.settingsFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B_FAT;
        lucidaFat.popupFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B;
        // lucidaFat.readableFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B_FAT;
        lucidaFat.readableFont = (Font *)&FONT_ECSANS_REGULAR_100BPI12PT;

        lucidaSkinny.mainMenuFont = (Font *)&FONT_LUCIDA_GRANDE10PT7B;
        lucidaSkinny.settingsFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B_SKINNY;
        lucidaSkinny.popupFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B_SKINNY;
        // lucidaSkinny.readableFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B_SKINNY;
        lucidaSkinny.readableFont = (Font *)&FONT_ECSANS_REGULAR_100BPI12PT;

        tahomaRegular.mainMenuFont = (Font *)&FONT_TAHOMA_REGULAR_FONT5PT7B;
        // tahomaRegular.readableFont = (Font *)&FONT_TAHOMA_REGULAR_FONT5PT7B;
        tahomaRegular.readableFont = (Font *)&FONT_ECSANS_REGULAR_100BPI12PT;

        tahomaLarge.mainMenuFont = (Font *)&FONT_TAHOMA_REGULAR_FONT6PT7B;
        tahomaLarge.settingsFont = (Font *)&FONT_TAHOMA_REGULAR_FONT5PT7B;
        tahomaLarge.popupFont = (Font *)&FONT_TAHOMA_REGULAR_FONT5PT7B;
        // tahomaLarge.readableFont = (Font *)&FONT_TAHOMA_REGULAR_FONT6PT7B;
        tahomaLarge.readableFont = (Font *)&FONT_ECSANS_REGULAR_100BPI12PT;

        currentFont = tahomaRegular;
    }

    void setFontFamily(FontFamily newFont) { currentFont = newFont; }

    auto getFontFamily() -> FontFamily { return currentFont; }

    FontFamily lucidaRegular;
    FontFamily lucidaFat;
    FontFamily lucidaSkinny;
    FontFamily tahomaRegular;
    FontFamily tahomaLarge;

    FontFamily currentFont;

protected:
};
