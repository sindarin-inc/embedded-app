#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <string>
#include <vector>

#include "Misc/Menu.hpp"
#include "Misc/MenuStyle.hpp"
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

class Menu {

public:
    Menu(MenuStyleFamily myStyle) : myStyle_(myStyle) { numMenuItems_ = 0; };

    Menu() { numMenuItems_ = 0; };

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
    virtual auto nextSelection(bool skipLast = false) -> bool;
    virtual auto prevSelection(bool skipLast = false) -> bool;

    // Includes the name of the item and it's number preceding it for easy display
    // Offset by two spaces
    auto getItemTitleAt(int index) -> std::string;
    void changeItemTitleAt(int index, std::string title);

    auto getItemAt(int index) -> MenuItem;

    auto getMyStyle() -> MenuStyleFamily { return myStyle_; }

protected:
    int numMenuItems_ = 0;
    std::vector<MenuItem> menuItems_;
    int currSelection_ = 0;
    int currMenuPage_ = 0;

    MenuStyleFamily myStyle_;
};

#endif
