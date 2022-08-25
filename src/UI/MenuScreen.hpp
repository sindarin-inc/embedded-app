#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <string>
#include <utility>
#include <vector>

#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class MenuItem {
public:
    MenuItem() = default;
    MenuItem(std::string title) : title(std::move(title)){};
    MenuItem(std::string title, std::function<void()> callback)
        : title(std::move(title)), callback(std::move(std::move(callback))){};

    std::string title;
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
    virtual void nextSelection();
    virtual void prevSelection();

    // Includes the name of the item and it's number preceding it for easy display
    // Offset by two spaces
    auto getItemTitleAt(int index) -> std::string;

    auto getItemAt(int index) -> MenuItem;

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

protected:
    int numMenuItems_ = 0;
    std::vector<MenuItem> menuItems_;
    int currSelection_ = 0;
    int curMenuPage_ = 0;
    bool firstLoad_ = true;

    Power &power_;
};

#endif
