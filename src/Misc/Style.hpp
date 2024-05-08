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
    Style() : currentFont(solSansSmall) {
        solSansSmall.mainMenuFont = static_cast<Font *>(&fontFace1);
        solSansSmall.popupFont = static_cast<Font *>(&fontFace0);
        solSansSmall.settingsFont = static_cast<Font *>(&fontFace0);
        solSansSmall.readableFont = static_cast<Font *>(&fontFace0);

        solSansMedium.mainMenuFont = static_cast<Font *>(&fontFace1);
        solSansMedium.popupFont = static_cast<Font *>(&fontFace0);
        solSansMedium.settingsFont = static_cast<Font *>(&fontFace0);
        solSansMedium.readableFont = static_cast<Font *>(&fontFace1);

        solSansLarge.mainMenuFont = static_cast<Font *>(&fontFace1);
        solSansLarge.popupFont = static_cast<Font *>(&fontFace0);
        solSansLarge.settingsFont = static_cast<Font *>(&fontFace0);
        solSansLarge.readableFont = static_cast<Font *>(&fontFace2);
    }

    void setFontFamily(FontFamily newFont) { currentFont = newFont; }

    [[nodiscard]] auto getFontFamily() const -> FontFamily { return currentFont; }

    FontFamily solSansSmall{};
    FontFamily solSansMedium{};
    FontFamily solSansLarge{};

    FontFamily currentFont{};

protected:
};

#undef IBMF_TEST