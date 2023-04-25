#include "Misc/Menu.hpp"

#include <utility>

#include "Misc/MenuStyle.hpp"
#include "Misc/Style.hpp"

void Menu::addMenuItem(const MenuItem &newItem) {
    menuItems_.push_back(newItem);
    numMenuItems_++;
}

void Menu::resetMenuItems() {
    menuItems_.clear();
    numMenuItems_ = 0;
    currSelection_ = 0;
    currMenuPage_ = 0;
}

void Menu::populateMenuItems(std::vector<MenuItem> &menuItems) {
    for (int i = currMenuPage_ * myStyle_.numberOfItemsPerPage;
         (i < (currMenuPage_ * myStyle_.numberOfItemsPerPage + myStyle_.numberOfItemsPerPage)) &&
         (i < numMenuItems_);
         i++) {
        menuItems.push_back(menuItems_[i]);
    }
}

auto Menu::getNumItems() -> int { return numMenuItems_; }

void Menu::setCurrSelection(int newSelection) {
    currSelection_ = newSelection;
    if (myStyle_.numberOfItemsPerPage == 0) {
        currMenuPage_ = 0;
    } else {
        currMenuPage_ = currSelection_ / myStyle_.numberOfItemsPerPage;
    }
}

auto Menu::getCurrSelection() -> int { return currSelection_; }

auto Menu::getCurrItem() -> MenuItem { return menuItems_[currSelection_]; }

auto Menu::updateCurrItemTitle(std::string newTitle) -> bool {
    if (menuItems_[currSelection_].title != newTitle) {
        menuItems_[currSelection_].title = std::move(newTitle);
        return true;
    }
    return false;
}

auto Menu::nextSelection(bool skipLast) -> bool {
    bool needsRefresh = false;

    int oldCurMenuPage = currMenuPage_;
    currSelection_ = (currSelection_ + 1) % numMenuItems_;
    if (skipLast && (currSelection_ == numMenuItems_ - 1)) {
        currSelection_ = 0;
    }
    if (myStyle_.numberOfItemsPerPage == 0) {
        currMenuPage_ = 0;
    } else {
        currMenuPage_ = currSelection_ / myStyle_.numberOfItemsPerPage;
    }
    // This makes sure we do a full refresh when the page changes
    if (oldCurMenuPage != currMenuPage_) {
        log_d("Page changed, refreshing");
        needsRefresh = true;
    }

    return needsRefresh;
}

auto Menu::prevSelection(bool skipLast) -> bool {
    bool needsRefresh = false;

    int oldCurMenuPage = currMenuPage_;
    // Wrap back around to the last item
    currSelection_ = (currSelection_ + numMenuItems_ - 1) % numMenuItems_;
    if (skipLast && (currSelection_ == numMenuItems_ - 1)) {
        currSelection_--;
    }
    if (myStyle_.numberOfItemsPerPage == 0) {
        currMenuPage_ = 0;
    } else {
        currMenuPage_ = currSelection_ / myStyle_.numberOfItemsPerPage;
    }
    // This makes sure we do a full refresh when the page changes
    if (oldCurMenuPage != currMenuPage_) {
        needsRefresh = true;
    }

    return needsRefresh;
}

// Includes the name of the item and it's number preceding it for easy display
// Offset by two spaces
auto Menu::getItemTitleAt(int index) -> std::string {
    std::string all;
    assert(index < numMenuItems_);
    all = "  " + menuItems_[index].title;
    return all;
}
void Menu::changeItemTitleAt(int index, std::string title) {
    assert(index < numMenuItems_);
    menuItems_[index].title = std::move(title);
}

auto Menu::getItemAt(int index) -> MenuItem {
    assert(index < numMenuItems_);
    return menuItems_[index];
}
