#pragma once

#include <Adafruit_GFX.h>

#include "UI/Fonts/LucidaGrande10pt7b.h"
#include "UI/Fonts/LucidaGrande14pt7b.h"
#include "UI/Fonts/LucidaGrande5pt7b.h"
#include "UI/Fonts/LucidaGrande6pt7b.h"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "UI/Fonts/Tahoma5pt7b.h"
#include "UI/Fonts/Tahoma6pt7b.h"

struct FontFamily {
    const GFXfont *settingsFont;
    const GFXfont *mainMenuFont;
    const GFXfont *popupFont;
    const GFXfont *readableFont;
};

class Style {
public:
    Style() {
        lucidaRegular.mainMenuFont = &LUCIDA_GRANDE10PT7B;
        lucidaRegular.settingsFont = &LUCIDA_GRANDE5PT7B;
        lucidaRegular.popupFont = &LUCIDA_GRANDE5PT7B;
        lucidaRegular.readableFont = &LUCIDA_GRANDE5PT7B;

        lucidaFat.mainMenuFont = &LUCIDA_GRANDE10PT7B;
        lucidaFat.settingsFont = &LUCIDA_GRANDE5PT7B_FAT;
        lucidaFat.popupFont = &LUCIDA_GRANDE5PT7B;
        lucidaFat.readableFont = &LUCIDA_GRANDE5PT7B_FAT;

        lucidaSkinny.mainMenuFont = &LUCIDA_GRANDE10PT7B;
        lucidaSkinny.settingsFont = &LUCIDA_GRANDE5PT7B_SKINNY;
        lucidaSkinny.popupFont = &LUCIDA_GRANDE5PT7B_SKINNY;
        lucidaSkinny.readableFont = &LUCIDA_GRANDE5PT7B_SKINNY;

        tahomaRegular.mainMenuFont = &TAHOMA_REGULAR_FONT10PT7B;
        tahomaRegular.settingsFont = &TAHOMA_REGULAR_FONT5PT7B;
        tahomaRegular.popupFont = &TAHOMA_REGULAR_FONT5PT7B;
        tahomaRegular.readableFont = &TAHOMA_REGULAR_FONT5PT7B;

        tahomaLarge.mainMenuFont = &TAHOMA_REGULAR_FONT10PT7B;
        tahomaLarge.settingsFont = &TAHOMA_REGULAR_FONT5PT7B;
        tahomaLarge.popupFont = &TAHOMA_REGULAR_FONT5PT7B;
        tahomaLarge.readableFont = &TAHOMA_REGULAR_FONT6PT7B;

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
