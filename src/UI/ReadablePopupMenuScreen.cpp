#include "ReadablePopupMenuScreen.hpp"

void ReadablePopupMenuScreen::setUpPopupMenu() {
    size_t place = renderer_.getCurrentPlace();
    size_t lengthOfFile = renderer_.size();

    float placeInTextPercentage = (float)place / (float)lengthOfFile * 100;

    std::string placeString = StringFormat("%.1f%%", placeInTextPercentage);
    std::string placeStringForBottom =
        StringFormat("%d/%d", SOL_PAGE_NUMBER(place), SOL_PAGE_NUMBER(lengthOfFile));

    popupMenuView_.setAuxText(placeStringForBottom);

    if (popupMenuView_.menuItemViews.size() == 0) {
        populatePopupMenu();
    }

    // Figure out if you should show the Archive menu item or the Unarchive menu item.
    bool archived = prefs_.readableArchivedChanged.get(readable_->shortenedUuid.c_str());
    if (archived) {
        archiveMenuItemView_->setHidden(true);
        unarchiveMenuItemView_->setHidden(false);
    } else {
        archiveMenuItemView_->setHidden(false);
        unarchiveMenuItemView_->setHidden(true);
    }
    popupMenuView_.setNeedsLayout();
    setFirstLoad();
}

void ReadablePopupMenuScreen::addMenuItem(const MenuItem &newItem) {
    Menu::addMenuItem(newItem);
    auto menuItemView = std::make_shared<MenuItemView>(display_, newItem);
    menuItemView->setCircleIndicator(true);
    popupMenuView_.menuItemViews.emplace_back(menuItemView);
}

void ReadablePopupMenuScreen::populatePopupMenu() {
    resetMenuItems();
    localStore_.loadToc(readable_);

    addMenuItem(MenuItem("Main Menu", [this]() { onMainMenu(); }));

    addMenuItem(MenuItem("Archive", [this]() {
        prefs_.readableArchivedChanged.set(readable_->shortenedUuid.c_str(), true);
        onMainMenu();
    }));
    archiveMenuItemView_ = popupMenuView_.menuItemViews.back();

    addMenuItem(MenuItem("Unarchive", [this]() {
        this->prefs_.readableArchivedChanged.remove(readable_->shortenedUuid.c_str());
        back();
    }));
    unarchiveMenuItemView_ = popupMenuView_.menuItemViews.back();

    addMenuItem(MenuItem("Bookmark", [this]() {
        onBookmarkToggle();
        back();
    }));

    if (readable_->hasSummary) {
        addMenuItem(MenuItem("Summary", [this]() {
            readableSummaryScreen_.setPlaceInReadable(renderer_.getCurrentPlace());
            readableSummaryScreen_.setFirstLoad();
            setCurrentScreen(readableSummaryScreen_);
        }));
    }

    addMenuItem(MenuItem("Brightness", [this]() {
        log_d("Setting current screen to brightness screen");
        lightingAdjustScreen_.setFirstLoad();
        setCurrentScreen(lightingAdjustScreen_);
    }));

    auto hasToc = readable_->toc.size() >= 1;
    log_d("hasToc = %d, readable_.toc.size: %zu", hasToc, readable_->toc.size());
    // If readable has a ToC with at least 1 element, show the ToC screen.
    // Otherwise, show the seek screen.
    if (hasToc) {
        addMenuItem(MenuItem("Contents", [this]() {
            readableTableOfContentsScreen_.loadMenuItems();
            readableTableOfContentsScreen_.setNearestChapterSelection(renderer_.getCurrentPlace());
            readableTableOfContentsScreen_.setFirstLoad();
            setCurrentScreen(readableTableOfContentsScreen_);
        }));
    } else {
        addMenuItem(MenuItem("Seek", [this]() {
            readableSeekScreen_.setFirstLoad();
            setCurrentScreen(readableSeekScreen_);
        }));
    }
}

void ReadablePopupMenuScreen::preselectArchiveMenuItem() {
    bool archived = prefs_.readableArchivedChanged.get(readable_->shortenedUuid.c_str());
    int index = 0;
    for (auto &menuItemView : popupMenuView_.menuItemViews) {
        if (menuItemView == (archived ? unarchiveMenuItemView_ : archiveMenuItemView_)) {
            setCurrSelection(index);
            break;
        }
        index++;
    }
}

void ReadablePopupMenuScreen::preselectFirstItem() { setCurrSelection(0); }

void ReadablePopupMenuScreen::preselectLastItem() { setCurrSelection(numMenuItems_ - 1); }

void ReadablePopupMenuScreen::processCommand(RemoteCommand command) {
    if (currentScreen_ != this) {
        currentScreen_->processCommand(command);
        return;
    }

    switch (command) {
    case RemoteCommand::LEFT:
        back();
        break;
    case RemoteCommand::ENTER:
        if (getCurrItem().callback != nullptr) {
            getCurrItem().callback();
        }
        break;
    case RemoteCommand::DOWN:
        do {
            nextSelection();
            setNeedsRerender();
        } while (popupMenuView_.menuItemViews[getCurrSelection()]->isHidden());
        break;
    case RemoteCommand::RIGHT:
    case RemoteCommand::BACK:
        back();
        break;
    case RemoteCommand::UP:
        do {
            prevSelection();
            setNeedsRerender();
        } while (popupMenuView_.menuItemViews[getCurrSelection()]->isHidden());
        break;
    case RemoteCommand::ANIMATION:
    default:
        break;
    }
}

void ReadablePopupMenuScreen::drawInRect(Rect rect) {}

auto ReadablePopupMenuScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(popupMenuView_, {{size.width / 2 - 50, size.height / 2 - 65}, {100, 130}},
                         LayoutOptionsAlignCenter);
    return size;
}

void ReadablePopupMenuScreen::render(DisplaySystem &display) {
    if (currentScreen_ != this) {
        currentScreen_->render(display);
        return;
    }

    setFrame({{0, 0}, {display.width(), display.height()}});
    layoutIfNeeded();

    if (firstLoad_) {
        // This displays the popup with a pre-AU4 refresh without the bullet
        // indiciating which menu item is selected.
        for (auto &menuItemView : popupMenuView_.menuItemViews) {
            menuItemView->setSelected(false);
        }

        popupMenuView_.updateTime();
        popupMenuView_.draw();

        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);

        firstLoad_ = false;
    } else {
        // NOTE: This Screen is quirky since it is rendered directly from ReadableScreen. The first
        // time it is rendered, it will be rendered twice from ReadableScreen, once with the text in
        // the background (using PARTIAL) and once without the text in the background (but using
        // AU4). This creates the effect of the popup happening on top of the text.
        std::shared_ptr<MenuItemView> selectedMenuItemView =
            popupMenuView_.menuItemViews[getCurrSelection()];
        for (auto &menuItemView : popupMenuView_.menuItemViews) {
            if (menuItemView && menuItemView == selectedMenuItemView) {
                menuItemView->setSelected(true);
            } else {
                menuItemView->setSelected(false);
            }
        }

        popupMenuView_.draw();

        display.updateDisplay(DisplaySystem::UpdateType::FAST_WITH_VCOM_NUDGE);
    }
};
