#pragma once

#include <Adafruit_GFX.h>

#include "UI/Fonts/Fonts.hpp"

struct MenuStyleFamily {
    Font *itemFont;
    Font *navigationFont;

    int xOffsetForArrows;
    int yOffsetForArrows;
    int arrowWidth;
    int arrowHeight;
    int arrowTextSpacing;

    int xOffsetForList;
    int yOffsetForList;

    int numberOfItemsPerPage;
};

class MenuStyle {
public:
    MenuStyle() {

        mainMenu.itemFont = static_cast<Font *>(&fontFace1);
        mainMenu.navigationFont = static_cast<Font *>(&fontFace0);

        mainMenu.xOffsetForArrows = 45;
        mainMenu.yOffsetForArrows = 45;
        mainMenu.arrowWidth = 5;
        mainMenu.arrowHeight = 10;
        mainMenu.arrowTextSpacing = 5;
        mainMenu.xOffsetForList = 15;
        mainMenu.yOffsetForList = 60;
        mainMenu.numberOfItemsPerPage = 4;

        settingsMenu.itemFont = static_cast<Font *>(&fontFace0);
        settingsMenu.navigationFont = static_cast<Font *>(&fontFace0);

        settingsMenu.xOffsetForArrows = 25;
        settingsMenu.yOffsetForArrows = 15;
        settingsMenu.arrowWidth = 5;
        settingsMenu.arrowHeight = 10;
        settingsMenu.arrowTextSpacing = 5;
        settingsMenu.xOffsetForList = 30;
        settingsMenu.yOffsetForList = 25;
        settingsMenu.numberOfItemsPerPage = 12;

        currentMenu = mainMenu;
    }

    void setMenuStyleFamily(MenuStyleFamily newMenu) { currentMenu = newMenu; }

    [[nodiscard]] auto getMenuStyleFamily() const -> MenuStyleFamily { return currentMenu; }

    // Not sure if we'll need the currentMenu, but we have it here if needed.
    MenuStyleFamily currentMenu{};
    MenuStyleFamily mainMenu{};
    MenuStyleFamily settingsMenu{};

protected:
};
