#include "ReadableTableOfContentsScreen.hpp"

auto ReadableTableOfContentsScreen::layout(LayoutContext &context, Size size) -> Size {
    const auto inset = 2;
    const auto spacing = 10;

    auto insetRect = context.setSublayoutFrame(
        {{inset, inset}, {size.width - 2 * inset, size.height - 2 * inset}},
        LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
        [this](LayoutContext &context, Size size) -> Size {
            int x = 0;
            int y = 0;
            log_i("ToC menu layout: %zu views in size %dpx x %d", menuItemViews_.size(), size.width,
                  size.height);

            for (auto &view : menuItemViews_) {
                auto viewFrame = context.setViewFrame(*view, {{x, y}, size},
                                                      LayoutOptionsSizeToFitVertical |
                                                          LayoutOptionsAlignCenterVertical);
                y += viewFrame.size.height + spacing;
            }

            return {size.width, y};
        });

    return {size.width, insetRect.size.height + 2 * inset};
};

void ReadableTableOfContentsScreen::drawInRect(Rect rect) {
    for (auto &view : menuItemViews_) {
        (*view).draw();
    }
}

void ReadableTableOfContentsScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    if (menuItemViews_.size() <= 0) {
        log_w("No views to draw in ReadableTableOfContentsScreen");
    }

    setNeedsLayout();
    setFrame({{0, 0}, {display.width(), display.height()}});

    if (firstLoad_) {
        PROFILE_START(ReadableTableOfContentsScreenLayoutAndDraw);
        menuItemsPage_.clear();
        menuItemViews_.clear();

        populateMenuItems(menuItemsPage_);

        for (auto &menuItem : menuItemsPage_) {
            auto menuItemView =
                std::make_shared<MenuItemView>(display, menuItem, myStyle_.navigationFont, false);
            menuItemViews_.push_back(menuItemView);
        }

        int numPages = getNumItems() / getMyStyle().numberOfItemsPerPage + 1;
        if (getNumItems() % getMyStyle().numberOfItemsPerPage == 0) {
            numPages--;
        }

        for (int i = 0; i < numPages; i++) {
            auto menuPageView = std::make_shared<BoxView>(display, 3);

            if (i == (getCurrSelection() / getMyStyle().numberOfItemsPerPage)) {
                menuPageView->fillColor = display.fg;
            }
            menuPageIndicatorViews_.push_back(menuPageView);
        }

        draw();
        PROFILE_END_LOG_US(ReadableTableOfContentsScreenLayoutAndDraw);
        display.updateDisplay(DisplaySystem::UpdateType::FULL);
    }

    std::shared_ptr<MenuItemView> selectedMenuItemView =
        menuItemViews_[getCurrSelection() % getMyStyle().numberOfItemsPerPage];
    for (auto &menuItemView : menuItemViews_) {
        if (menuItemView && menuItemView == selectedMenuItemView) {
            menuItemView->setSelected(true);
        } else {
            menuItemView->setSelected(false);
        }
    }

    // Redraw with a different vcom setting for the menu item bullet
    draw();

    display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);

    firstLoad_ = false;
    needsRerender = false;
}

void ReadableTableOfContentsScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::PREV:
        if (prevSelection()) {
            firstLoad_ = true;
        }
        needsRerender = true;
        break;
    case RemoteCommand::NEXT:
        if (nextSelection()) {
            firstLoad_ = true;
        }
        needsRerender = true;
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::CHANNEL_PLUS:
        setPlacemark();
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::CHANNEL_MINUS:
        if (backCallback != nullptr) {
            firstLoad_ = true;
            backCallback();
        }
        needsRerender = true;
        break;
    case RemoteCommand::ANIMATION:
        needsRerender = false;
        break;
    default:
        break;
    }
}

void ReadableTableOfContentsScreen::setPlacemark() {
    auto chapter = readable_.toc[getCurrSelection()];
    readable_.place = chapter.txtOffset;
    renderer_.setCurrentPlace(readable_.place);
    prefs_.readablePlace.set(readable_.shortenedUuid.c_str(), readable_.place);
    prefs_.readablePlaceChanged.set(readable_.shortenedUuid.c_str(), true);
}
