#pragma once

#include <Adafruit_GFX.h>

#include "UI/Fonts/Tahoma10pt7b.h"
#include "UI/Fonts/Tahoma5pt7b.h"
#include "UI/Fonts/Tahoma6pt7b.h"
#include "UI/Fonts/Tahoma8pt7b.h"

struct MenuStyleFamily {
    const GFXfont *itemFont;
    const GFXfont *navigationFont;

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
        mainMenu.itemFont = &TAHOMA_REGULAR_FONT6PT7B;
        mainMenu.navigationFont = &TAHOMA_REGULAR_FONT5PT7B;

        mainMenu.xOffsetForArrows = 45;
        mainMenu.yOffsetForArrows = 45;
        mainMenu.arrowWidth = 5;
        mainMenu.arrowHeight = 10;
        mainMenu.arrowTextSpacing = 5;
        mainMenu.xOffsetForList = 15;
        mainMenu.yOffsetForList = 60;
        mainMenu.numberOfItemsPerPage = 5;

        settingsMenu.itemFont = &TAHOMA_REGULAR_FONT5PT7B;
        settingsMenu.navigationFont = &TAHOMA_REGULAR_FONT5PT7B;

        settingsMenu.xOffsetForArrows = 45;
        settingsMenu.yOffsetForArrows = 45;
        settingsMenu.arrowWidth = 5;
        settingsMenu.arrowHeight = 10;
        settingsMenu.arrowTextSpacing = 5;
        settingsMenu.xOffsetForList = 20;
        settingsMenu.yOffsetForList = 40;
        settingsMenu.numberOfItemsPerPage = 8;

        currentMenu = mainMenu;
    }

    void setMenuStyleFamily(MenuStyleFamily newMenu) { currentMenu = newMenu; }

    auto getMenuStyleFamily() -> MenuStyleFamily { return currentMenu; }

    // Not sure if we'll need the currentMenu, but we have it here if needed.
    MenuStyleFamily currentMenu;
    MenuStyleFamily mainMenu;
    MenuStyleFamily settingsMenu;

protected:
};
