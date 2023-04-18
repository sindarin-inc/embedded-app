#include "MainMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "Misc/StringUtil.hpp"
#include "Misc/Style.hpp"

#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

void MainMenuScreen::addSubMenu(Menu *newSubMenu) { subMenus_.push_back(newSubMenu); }

auto MainMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    const auto inset = 3;
    const auto spacing = 5;

    context.setSublayoutFrame(
        {{inset, inset}, {size.width - 2 * inset, 20 - 2 * inset}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignCenterHorizontal,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            int viewCount = topBarViews_.size();
            int boxWidth = (size.width - (viewCount - 1) * spacing) / viewCount;
            Size boxSize = {boxWidth, size.height};

            int place = 0;

            for (auto &view : topBarViews_) {
                context.setViewFrame(*view, {{x, y}, boxSize},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
                if (place == currentSelection_) {
                    context.setViewFrame(topBarSelectionView_, {{x, y}, boxSize});
                }
                x += boxWidth + spacing;
                place++;
            };

            return {x, size.height};
        });

    const auto mainInset = 2;
    const auto mainSpacing = 10;

    context.setSublayoutFrame(
        // The -20 is to leave room for the paging circles at the right hand side of the view
        {{mainInset, mainInset}, {size.width - 2 * mainInset - 20, size.height - 2 * mainInset}},
        LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            int viewCount = menuItemViews_.size();
            int boxHeight = (size.height - (viewCount - 1) * mainSpacing) / viewCount;
            Size boxSize = {size.width, boxHeight};

            for (auto &view : menuItemViews_) {
                auto viewFrame =
                    context.setViewFrame(*view, {{x, y}, boxSize},
                                         LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenter);
                y += viewFrame.size.height + mainSpacing;
            }

            return {size.width, y};
        });

    context.setSublayoutFrame(
        // The -5 is to leave room for the paging circles at the right hand side of the view
        {{size.width - mainInset - 5, mainInset}, {10, size.height - 2 * mainInset}},
        LayoutOptionsSizeToFit | LayoutOptionsAlignCenterVertical | LayoutOptionsAlignRight,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            Size boxSize = {5, 5};
            for (auto &view : menuPageViews_) {
                auto viewFrame = context.setViewFrame(
                    *view, {{x, y}, boxSize}, LayoutOptionsSizeToFit | LayoutOptionsAlignRight);
                y += viewFrame.size.height + 5;
            }

            return {size.width, y};
        });

    context.setViewFrame(clockBatteryView_, {{0, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                             LayoutOptionsAlignCenterHorizontal);

    return size;
}

void MainMenuScreen::drawInRect(Rect rect) {
    for (auto &view : topBarViews_) {
        (*view).draw();
    }

    for (auto &view : menuItemViews_) {
        (*view).draw();
    }

    if (menuPageViews_.size() > 1) {
        for (auto &view : menuPageViews_) {
            (*view).draw();
        }
    }

    topBarSelectionView_.draw();
    clockBatteryView_.draw();
}

// This largely mirrors the superclass implementation, but with enough
// changes that we need to do our own thing here.
void MainMenuScreen::render(DisplaySystem &display) {
    setNeedsLayout();

    // For the main menu, we want to use the entire screen and ignore the user settings for reading
    // box.
    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {
        topBarViews_.clear();
        log_i("Menuscreen render (selected = %s)",
              subMenus_[currentSelection_]->getMenuName().c_str());

        for (auto &subMenu : subMenus_) {
            auto topBarView = std::make_shared<SingleLineTextView>(display, subMenu->getMenuName());
            topBarViews_.push_back(topBarView);
        }

        // Populate the menu items with the correct menus for the current subMenu
        menuItemViews_.clear();
        menuItems_.clear();
        menuPageViews_.clear();

        subMenus_[currentSelection_]->populateMenuItems(menuItems_);

        for (auto &menuItem : menuItems_) {
            auto menuItemView = std::make_shared<MenuItemView>(
                display, menuItem,
                ((subMenus_[currentSelection_]->getMenuName().compare("Books") == 0) ||
                 (subMenus_[currentSelection_]->getMenuName().compare("Bible") == 0))
                    ? myStyle_.itemFont
                    : myStyle_.navigationFont,
                (subMenus_[currentSelection_]->getMenuName().compare("Articles") == 0) ? true
                                                                                       : false);
            menuItemViews_.push_back(menuItemView);
        }

        int numPages = subMenus_[currentSelection_]->getNumItems() /
                           subMenus_[currentSelection_]->getMyStyle().numberOfItemsPerPage +
                       1;

        if (subMenus_[currentSelection_]->getNumItems() %
                subMenus_[currentSelection_]->getMyStyle().numberOfItemsPerPage ==
            0) {
            numPages--;
        }

        for (int i = 0; i < numPages; i++) {
            auto menuPageView = std::make_shared<BoxView>(display, 3);

            if (i == (subMenus_[currentSelection_]->getCurrSelection() /
                      subMenus_[currentSelection_]->getMyStyle().numberOfItemsPerPage)) {
                menuPageView->fillColor = display.fg;
            }
            menuPageViews_.push_back(menuPageView);
        }

        draw();
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    }

    std::shared_ptr<MenuItemView> selectedMenuItemView =
        menuItemViews_[subMenus_[currentSelection_]->getCurrSelection() %
                       subMenus_[currentSelection_]->getMyStyle().numberOfItemsPerPage];
    for (auto &menuItemView : menuItemViews_) {
        if (menuItemView && menuItemView == selectedMenuItemView) {
            menuItemView->setSelected(true);
        } else {
            menuItemView->setSelected(false);
        }
    }

    draw();

    display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);

    firstLoad_ = false;
}

void MainMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
        if (subMenus_[currentSelection_]->prevSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::NEXT:
        if (subMenus_[currentSelection_]->nextSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
        }
        setNeedsRerender();
        break;
    // Long press down on the TT should count as enter since we don't have the
    // 5th button
    case RemoteCommand::ENTER:
    case RemoteCommand::STOP:
        if (subMenus_[currentSelection_]->getCurrItem().callback != nullptr) {
            subMenus_[currentSelection_]->getCurrItem().callback();
        }
        break;
    case RemoteCommand::CHANNEL_PLUS:
        // This will switch between the menus
        currentSelection_++;
        currentSelection_ = currentSelection_ % topBarViews_.size();
        firstLoad_ = true;
        setNeedsRerender();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        // This will switch between the menus
        currentSelection_ = currentSelection_ + topBarViews_.size() - 1;
        currentSelection_ = currentSelection_ % topBarViews_.size();
        firstLoad_ = true;
        setNeedsRerender();
        break;
    case RemoteCommand::ANIMATION:
        break;
    default:
        break;
    }
}