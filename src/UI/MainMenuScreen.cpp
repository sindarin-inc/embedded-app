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

    // Layout the top bar
    auto topBarRect = context.setSublayoutFrame(
        {{inset, inset}, {size.width, 20 - 2 * inset}}, LayoutOptionsNone,
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

            return size;
        });

    // Then layout the clock/battery view on the bottom
    auto clockBatteryRect = context.setViewFrame(clockBatteryView_, {{0, 0}, size},
                                                 LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                                                     LayoutOptionsAlignCenterHorizontal);

    // The remaining space is available for menu items
    auto mainRect =
        Rect{{0, topBarRect.bottom()}, {size.width, clockBatteryRect.top() - topBarRect.bottom()}};
    mainRect = mainRect.inset(2);

    // Layout the paging bubbles
    auto pagingRect = context.setSublayoutFrame(
        mainRect,
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
            y -= 5;

            return {boxSize.width, y};
        });

    // Layout the menu items in the mainRect, but minus the paging bubbles
    mainRect.size.width = mainRect.size.width - pagingRect.size.width - 5;
    context.setSublayoutFrame(
        mainRect, LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int y = 0;
            const auto mainSpacing = 8;
            for (auto &view : menuItemViews_) {
                if (view->isHidden()) {
                    continue;
                }
                auto viewFrame = context.setViewFrame(*view, {{0, y}, {size.width, 0}},
                                                      LayoutOptionsSizeToFitVertical);
                y += viewFrame.size.height + mainSpacing;
            }
            y -= mainSpacing;

            return {size.width, y};
        });

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
    // log_i("Menuscreen render (selected = %s)",
    // subMenus_[currentSelection_]->getMenuName().c_str());

    // For the main menu, we want to use the entire screen and ignore the user settings for
    // reading box.
    setFrame(
        {{display.xAbsolute(display.defaultXPadding), display.yAbsolute(display.defaultYPadding)},
         {display.defaultScreenWidth - 2 * display.defaultXPadding,
          display.defaultScreenHeight - 2 * display.defaultYPadding}});

    // We must explicitly set this in case of toggle between dark mode and not dark mode
    topBarSelectionView_.strokeColor = display.fg;

    // On the first load, we want to only render the items in the menu because we are going
    // to do a partial refresh with the selector button at a lower/higher vcom to reduce
    // ghosting.
    if (firstLoad_) {
        setNeedsLayout();

        if (topBarViews_.size() == 0) {
            for (auto &subMenu : subMenus_) {
                auto topBarView =
                    std::make_shared<SingleLineTextView>(display, subMenu->getMenuName());
                topBarViews_.push_back(topBarView);
            }
        }

        // Only update the time on these partial refreshes
        clockBatteryView_.updateTime();

        // Populate the menu items with the correct menus for the current subMenu
        menuItems_.clear();
        menuPageViews_.clear();

        subMenus_[currentSelection_]->populateMenuItems(menuItems_);

        for (int i = 0; i < menuItems_.size(); i++) {
            auto &menuItem = menuItems_[i];
            Font *font = ((subMenus_[currentSelection_]->getMenuName() == "Books") ||
                          (subMenus_[currentSelection_]->getMenuName() == "Bible"))
                             ? myStyle_.itemFont
                             : myStyle_.navigationFont;
            bool multiline =
                (subMenus_[currentSelection_]->getMenuName() == "Articles") ? true : false;
            if (i >= menuItemViews_.size()) {
                menuItemViews_.emplace_back(std::make_shared<MenuItemView>(display));
            }
            menuItemViews_[i]->setMenuItem(menuItem, font, multiline);
            menuItemViews_[i]->setSelected(false);
            menuItemViews_[i]->setHidden(false);
        }
        // Hide any menu items that are not in the current menu
        for (int i = menuItems_.size(); i < menuItemViews_.size(); i++) {
            menuItemViews_[i]->setHidden(true);
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
        // display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
        //                       DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE, {24, 50, 32,
        //                       205});
    }

    // If we want to try using the alt update mechanism here, we should move this code above the
    // update, and remove the second update
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
    //}

    // This handles the case where we are in the main menu right after the
    // onboarding stuff.
    display.clearDontChangeRefreshType();
    firstLoad_ = false;
}

void MainMenuScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        if (subMenus_[currentSelection_]->prevSelection()) {
            firstLoad_ = true;
        } else {
            firstLoad_ = false;
        }
        setNeedsRerender();
        break;
    case RemoteCommand::DOWN:
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
        if (subMenus_[currentSelection_]->getCurrItem().callback != nullptr) {
            subMenus_[currentSelection_]->getCurrItem().callback();
        }
        break;
    case RemoteCommand::RIGHT:
        // This will switch between the menus
        currentSelection_++;
        currentSelection_ = currentSelection_ % topBarViews_.size();
        firstLoad_ = true;
        setNeedsRerender();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        // This will switch between the menus
        currentSelection_ = currentSelection_ + topBarViews_.size() - 1;
        currentSelection_ = currentSelection_ % topBarViews_.size();
        firstLoad_ = true;
        setNeedsRerender();
        break;
    case RemoteCommand::ANIMATION:
    default:
        break;
    }
}