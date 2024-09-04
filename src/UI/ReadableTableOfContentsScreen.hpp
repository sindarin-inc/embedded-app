#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Misc/Menu.hpp"
#include "Misc/MenuStyle.hpp"
#include "Misc/Telemetry.hpp"
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
    ReadableTableOfContentsScreen(PreferencesStore &prefs, std::shared_ptr<Readable> readable,
                                  Renderer &renderer, DisplaySystem &display, Telemetry &telemetry)
        : EinkView(display), prefs_(prefs), readable_(std::move(readable)), renderer_(renderer),
          display_(display), telemetry_(telemetry), backLabelView_(display, "Dismiss") {
        numMenuItems_ = 0;
        MenuStyle styleFamilies;
        myStyle_ = styleFamilies.mainMenu;
        myStyle_.numberOfItemsPerPage = 11;
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

    void loadMenuItems();

    void setPlacemark();
    void setFirstLoad() { firstLoad_ = true; }
    void setNearestChapterSelection(const std::shared_ptr<Place> &placeInFile);

protected:
    PreferencesStore &prefs_;
    std::shared_ptr<Readable> readable_;
    Renderer &renderer_;
    DisplaySystem &display_;
    Telemetry &telemetry_;
    std::vector<MenuItem> menuItemsPage_;
    std::vector<std::shared_ptr<BoxView>> menuPageIndicatorViews_;
    std::vector<std::shared_ptr<MenuItemView>> menuItemViews_;
    bool firstLoad_ = true;

    ArrowLabelView backLabelView_;
};
