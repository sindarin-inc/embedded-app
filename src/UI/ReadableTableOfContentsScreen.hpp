#pragma once

#include <memory>
#include <vector>

#include "Misc/Menu.hpp"
#include "Misc/MenuStyle.hpp"
#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MenuItemView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class ReadableTableOfContentsScreen : public Screen, public Menu, public EinkView {
public:
    ReadableTableOfContentsScreen(PreferencesStore &prefs, Readable &readable, Renderer &renderer,
                                  DisplaySystem &display)
        : Menu(), EinkView(display), prefs_(prefs), readable_(readable), renderer_(renderer),
          display_(display), upperLeftTextView_(display, upperLeftText_), leftArrowView_(display) {
        numMenuItems_ = 0;
        MenuStyle styleFamilies;
        myStyle_ = styleFamilies.mainMenu;
        myStyle_.numberOfItemsPerPage = 8;
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

    void setPlacemark();
    void setFirstLoad() { firstLoad_ = true; }
    void setNearestChapterSelection(unsigned long placeInFile);

protected:
    PreferencesStore &prefs_;
    Readable &readable_;
    Renderer &renderer_;
    DisplaySystem &display_;
    std::vector<MenuItem> menuItemsPage_;
    std::vector<std::shared_ptr<BoxView>> menuPageIndicatorViews_;
    std::vector<std::shared_ptr<MenuItemView>> menuItemViews_;
    bool firstLoad_ = true;

    std::string upperLeftText_ = "Dismiss";
    SingleLineTextView upperLeftTextView_;

    ArrowView leftArrowView_;
};
