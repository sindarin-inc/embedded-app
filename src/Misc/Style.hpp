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
        lucidaRegular.mainMenuFont = static_cast<Font *>(&fontLucidaGrandE10PT7B);
        lucidaRegular.settingsFont = static_cast<Font *>(&fontLucidaGrandE5PT7B);
        lucidaRegular.popupFont = static_cast<Font *>(&fontLucidaGrandE5PT7B);
#if !IBMF_TEST
        lucidaRegular.readableFont = static_cast<Font *>(&FONT_LUCIDA_GRANDE5PT7B);
#else
        lucidaRegular.readableFont = static_cast<Font *>(&ibmf75Face1);
#endif

        lucidaFat.mainMenuFont = static_cast<Font *>(&fontLucidaGrandE10PT7B);
        lucidaFat.settingsFont = static_cast<Font *>(&fontLucidaGrandE5PT7BFat);
        lucidaFat.popupFont = static_cast<Font *>(&fontLucidaGrandE5PT7B);
#if !IBMF_TEST
        lucidaFat.readableFont = static_cast<Font *>(&FONT_LUCIDA_GRANDE5PT7B_FAT);
#else
        lucidaFat.readableFont = static_cast<Font *>(&ibmf75Face1);
#endif

        lucidaSkinny.mainMenuFont = static_cast<Font *>(&fontLucidaGrandE10PT7B);
        lucidaSkinny.settingsFont = static_cast<Font *>(&fontLucidaGrandE5PT7BSkinny);
        lucidaSkinny.popupFont = static_cast<Font *>(&fontLucidaGrandE5PT7BSkinny);
#if !IBMF_TEST
        lucidaSkinny.readableFont = static_cast<Font *>(&FONT_LUCIDA_GRANDE5PT7B_SKINNY);
#else
        lucidaSkinny.readableFont = static_cast<Font *>(&ibmf75Face1);
#endif

        tahomaTiny.mainMenuFont = static_cast<Font *>(&fontTahomaRegularFonT5PT7B);
        tahomaTiny.settingsFont = static_cast<Font *>(&fontTahomaRegularFonT5PT7B);
        tahomaTiny.popupFont = static_cast<Font *>(&fontTahomaRegularFonT5PT7B);
#if !IBMF_TEST
        tahomaRegular.readableFont = static_cast<Font *>(&FONT_TAHOMA_REGULAR_FONT5PT7B);
#else
        tahomaTiny.readableFont = static_cast<Font *>(&ibmf75Face0);
#endif

        tahomaRegular.mainMenuFont = static_cast<Font *>(&fontTahomaRegularFonT5PT7B);
        tahomaRegular.settingsFont = static_cast<Font *>(&fontTahomaRegularFonT5PT7B);
        tahomaRegular.popupFont = static_cast<Font *>(&fontTahomaRegularFonT5PT7B);
#if !IBMF_TEST
        tahomaRegular.readableFont = static_cast<Font *>(&FONT_TAHOMA_REGULAR_FONT5PT7B);
#else
        tahomaRegular.readableFont = static_cast<Font *>(&ibmf75Face1);
#endif

        tahomaLarge.mainMenuFont = static_cast<Font *>(&fontTahomaRegularFonT6PT7B);
        tahomaLarge.settingsFont = static_cast<Font *>(&fontTahomaRegularFonT5PT7B);
        tahomaLarge.popupFont = static_cast<Font *>(&fontTahomaRegularFonT5PT7B);
#if !IBMF_TEST
        tahomaLarge.readableFont = static_cast<Font *>(&FONT_TAHOMA_REGULAR_FONT6PT7B);
#else
        tahomaLarge.readableFont = static_cast<Font *>(&ibmf75Face2);
#endif

        tahomaHuge.mainMenuFont = static_cast<Font *>(&fontTahomaRegularFonT6PT7B);
        tahomaHuge.settingsFont = static_cast<Font *>(&fontTahomaRegularFonT5PT7B);
        tahomaHuge.popupFont = static_cast<Font *>(&fontTahomaRegularFonT5PT7B);
#if !IBMF_TEST
        tahomaHuge.readableFont = static_cast<Font *>(&FONT_TAHOMA_REGULAR_FONT6PT7B);
#else
        tahomaHuge.readableFont = static_cast<Font *>(&ibmf75Face3);
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
    FontFamily tahomaTiny;
    FontFamily tahomaHuge;

    FontFamily currentFont;

protected:
};

#undef IBMF_TEST