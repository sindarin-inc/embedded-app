#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>
#include <utility>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

#define MAX_NUM_MENU_ITEMS 10

class MenuItem {
public:
    MenuItem() = default;
    ;
    MenuItem(String title) : title(std::move(title)){};
    MenuItem(String title, std::function<void()> callback)
        : title(std::move(title)), callback(std::move(std::move(callback))){};

    String title;
    // Called when this menu item is selected
    std::function<void()> callback;
};

class MenuScreen : public Screen {

public:
    MenuScreen(Power &power) : power_(power) { numMenuItems_ = 0; };

    void addMenuItem(const MenuItem &newItem);

    void resetMenuItems();

    auto getNumItems() -> int;

    void setCurrSelection(int newSelection);

    auto getCurrSelection() -> int;

    auto getCurrItem() -> MenuItem;

    // Returns 1 if the selection was updated, 0 otherwise
    // Helps client to know if it needs to refresh (at beginning or end of menu)
    virtual auto nextSelection() -> int;
    virtual auto prevSelection() -> int;

    // Includes the name of the item and it's number preceding it for easy display
    // Offset by two spaces
    auto getItemTitleAt(int index) -> String;

    auto getItemAt(int index) -> MenuItem;

    // Easy \n separated list of the items in the menu that can simply be passed
    // to a display. May want to handle this in a Renderer? But for now, this
    // is simple enough.
    auto getWholeMenuAsString() -> String;

    // Allows the client to determine how many items from the menu it wants and
    // to generate a string accordingly.
    // Note it's up to the client to figure out what to do if the current
    // selection is outside of this range.
    auto getMenuRangeAsString(int startItem, int endItem) -> String;

    auto getPaginatedMenuString() -> String;

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

protected:
    int numMenuItems_ = 0;
    std::array<MenuItem, MAX_NUM_MENU_ITEMS> menuItems_;
    int currSelection_ = 0;
    int curMenuPage_ = 0;

    Power &power_;
};

#endif
