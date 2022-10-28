#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <string>
#include <utility>
#include <vector>

#include "Inputs/RemoteInput.hpp"
#include "Misc/MenuStyle.hpp"
#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class MenuItem {
public:
    MenuItem() = default;
    MenuItem(std::string title) : title(std::move(title)){};
    MenuItem(std::string title, std::function<void()> callback)
        : title(std::move(title)), callback(std::move(callback)){};

    std::string title;
    // Called when this menu item is selected
    std::function<void()> callback;
};

class MenuScreen : public Screen {

public:
    MenuScreen(Power &power, MenuStyleFamily myStyle) : power_(power), myStyle_(myStyle) {
        numMenuItems_ = 0;
    };

    MenuScreen(Power &power) : power_(power) { numMenuItems_ = 0; };

    void addMenuItem(const MenuItem &newItem);

    void resetMenuItems();

    auto getNumItems() -> int;

    void setCurrSelection(int newSelection);

    auto getCurrSelection() -> int;

    auto getCurrItem() -> MenuItem;

    // Returns 1 if the selection was updated, 0 otherwise
    // Helps client to know if it needs to refresh (at beginning or end of menu)
    // The optional argument is if we want to skip over the last element in the menu
    // Mostly this is for the main menu and settings menu until those are rewitten.
    virtual void nextSelection(bool skipLast = false);
    virtual void prevSelection(bool skipLast = false);

    // Includes the name of the item and it's number preceding it for easy display
    // Offset by two spaces
    auto getItemTitleAt(int index) -> std::string;

    auto getItemAt(int index) -> MenuItem;

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    void setFirstLoad() { firstLoad_ = true; }

    void setExtraText(std::string extra) { extraTextBelowMenu_ = std::move(extra); }

    void drawMenuItems(DisplaySystem &display, bool withBullet, bool withLastItem = true);

    auto getMyStyle() -> MenuStyleFamily { return myStyle_; }

    void showTemporaryMessage(DisplaySystem &display, const std::string &message,
                              int duration = 2000, bool rerenderAfter = true);

protected:
    void drawArrowsAndSelections(DisplaySystem &display);
    int numMenuItems_ = 0;
    std::vector<MenuItem> menuItems_;
    int currSelection_ = 0;
    int currMenuPage_ = 0;
    bool firstLoad_ = true;
#if EINKET013TT1
    int oldVcom_ = 0;
#endif

    std::string upperLeftText_ = "Back";
    std::string lowerRightText_ = "Select";

    std::string extraTextBelowMenu_ = "";

    Power &power_;
    MenuStyleFamily myStyle_;
};

#endif
