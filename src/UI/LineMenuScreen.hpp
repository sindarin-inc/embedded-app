#pragma once

#include <memory>
#include <vector>

#include "Misc/Menu.hpp"
#include "Misc/MenuStyle.hpp"
#include "Models/Readable.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/StackView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class LineMenuScreen : public Screen, public Menu, public EinkView {
public:
    LineMenuScreen(Readable &readable, PreferencesStore &prefs, DisplaySystem &display,
                   MenuStyleFamily menuStyle)
        : Menu(menuStyle), EinkView(display), display_(display) {
        numMenuItems_ = 0;
    }
    LineMenuScreen(Readable &readable, PreferencesStore &prefs, DisplaySystem &display)
        : Menu(), EinkView(display), display_(display) {
        numMenuItems_ = 0;
        MenuStyle styleFamilies;
        myStyle_ = styleFamilies.mainMenu;
    }

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    void resetMenuItems() override {
        Menu::resetMenuItems();
        menuItemViews_.clear();
        firstLoad_ = true;
    }

    void addMenuItem(const MenuItem &newItem) override {
        Menu::addMenuItem(newItem);
        auto menuItemView = std::make_shared<MenuItemView>(display_, newItem);
        menuItemViews_.push_back(menuItemView);
    }

protected:
    DisplaySystem &display_;
    std::vector<std::shared_ptr<MenuItemView>> menuItemViews_;
    bool firstLoad_ = true;
};
