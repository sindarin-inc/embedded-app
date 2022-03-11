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
        lucindaRegular.mainMenuFont = &LUCIDA_GRANDE10PT7B;
        lucindaRegular.settingsFont = &LUCIDA_GRANDE5PT7B;
        lucindaRegular.popupFont = &LUCIDA_GRANDE5PT7B;
        lucindaRegular.readableFont = &LUCIDA_GRANDE5PT7B;

        lucindaFat.mainMenuFont = &LUCIDA_GRANDE10PT7B;
        lucindaFat.settingsFont = &LUCIDA_GRANDE5PT7B_FAT;
        lucindaFat.popupFont = &LUCIDA_GRANDE5PT7B;
        lucindaFat.readableFont = &LUCIDA_GRANDE5PT7B_FAT;

        lucindaSkinny.mainMenuFont = &LUCIDA_GRANDE10PT7B;
        lucindaSkinny.settingsFont = &LUCIDA_GRANDE5PT7B_SKINNY;
        lucindaSkinny.popupFont = &LUCIDA_GRANDE5PT7B_SKINNY;
        lucindaSkinny.readableFont = &LUCIDA_GRANDE5PT7B_SKINNY;

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

    FontFamily lucindaRegular;
    FontFamily lucindaFat;
    FontFamily lucindaSkinny;
    FontFamily tahomaRegular;
    FontFamily tahomaLarge;

    FontFamily currentFont;

protected:
};
