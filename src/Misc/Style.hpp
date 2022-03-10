#pragma once

#include <Adafruit_GFX.h>

#include "UI/Fonts/LucidaGrande10pt7b.h"
#include "UI/Fonts/LucidaGrande14pt7b.h"
#include "UI/Fonts/LucidaGrande5pt7b.h"
#include "UI/Fonts/LucidaGrande6pt7b.h"
#include "UI/Fonts/Tahoma10pt7b.h"
#include "UI/Fonts/Tahoma5pt7b.h"
#include "UI/Fonts/Tahoma6pt7b.h"

struct fontFamily {
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

        tahomaRegular.mainMenuFont = &Tahoma_Regular_font10pt7b;
        tahomaRegular.settingsFont = &Tahoma_Regular_font5pt7b;
        tahomaRegular.popupFont = &Tahoma_Regular_font5pt7b;
        tahomaRegular.readableFont = &Tahoma_Regular_font5pt7b;

        tahomaLarge.mainMenuFont = &Tahoma_Regular_font10pt7b;
        tahomaLarge.settingsFont = &Tahoma_Regular_font5pt7b;
        tahomaLarge.popupFont = &Tahoma_Regular_font5pt7b;
        tahomaLarge.readableFont = &Tahoma_Regular_font6pt7b;

        currentFont = tahomaRegular;
    }

    void setFontFamily(struct fontFamily newFont) { currentFont = newFont; }

    struct fontFamily getFontFamily() {
        return currentFont;
    }

    struct fontFamily lucindaRegular;
    struct fontFamily lucindaFat;
    struct fontFamily lucindaSkinny;
    struct fontFamily tahomaRegular;
    struct fontFamily tahomaLarge;

    struct fontFamily currentFont;

protected:
};