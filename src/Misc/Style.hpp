#pragma once

#include <Adafruit_GFX.h>

#include "UI/Fonts/Fonts.hpp"

struct FontFamily {
    Font *settingsFont;
    Font *mainMenuFont;
    Font *popupFont;
    Font *readableFont;
};

#define IBMF_TEST 1

class Style {
public:
    Style() {
        lucidaRegular.mainMenuFont = (Font *)&FONT_LUCIDA_GRANDE10PT7B;
        lucidaRegular.settingsFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B;
        lucidaRegular.popupFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B;
#if !IBMF_TEST
        lucidaRegular.readableFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B;
#else
        lucidaRegular.readableFont = (Font *)&fontTahoma75BPI10PT;
#endif

        lucidaFat.mainMenuFont = (Font *)&FONT_LUCIDA_GRANDE10PT7B;
        lucidaFat.settingsFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B_FAT;
        lucidaFat.popupFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B;
#if !IBMF_TEST
        lucidaFat.readableFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B_FAT;
#else
        lucidaFat.readableFont = (Font *)&fontTahoma75BPI10PT;
#endif

        lucidaSkinny.mainMenuFont = (Font *)&FONT_LUCIDA_GRANDE10PT7B;
        lucidaSkinny.settingsFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B_SKINNY;
        lucidaSkinny.popupFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B_SKINNY;
#if !IBMF_TEST
        lucidaSkinny.readableFont = (Font *)&FONT_LUCIDA_GRANDE5PT7B_SKINNY;
#else
        lucidaSkinny.readableFont = (Font *)&fontTahoma75BPI10PT;
#endif

        tahomaRegular.mainMenuFont = (Font *)&FONT_TAHOMA_REGULAR_FONT5PT7B;
        tahomaRegular.settingsFont = (Font *)&TAHOMA_REGULAR_FONT5PT7B;
        tahomaRegular.popupFont = (Font *)&TAHOMA_REGULAR_FONT5PT7B;
#if !IBMF_TEST
        tahomaRegular.readableFont = (Font *)&FONT_TAHOMA_REGULAR_FONT5PT7B;
#else
        tahomaRegular.readableFont = (Font *)&fontTahoma75BPI10PT;
#endif

        tahomaLarge.mainMenuFont = (Font *)&FONT_TAHOMA_REGULAR_FONT6PT7B;
        tahomaLarge.settingsFont = (Font *)&FONT_TAHOMA_REGULAR_FONT5PT7B;
        tahomaLarge.popupFont = (Font *)&FONT_TAHOMA_REGULAR_FONT5PT7B;
#if !IBMF_TEST
        tahomaLarge.readableFont = (Font *)&FONT_TAHOMA_REGULAR_FONT6PT7B;
#else
        tahomaLarge.readableFont = (Font *)&fontTahoma75BPI10PT;
#endif

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

#undef IBMF_TEST