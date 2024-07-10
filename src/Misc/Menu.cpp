#include "Misc/Menu.hpp"

#include <utility>

#include "Misc/MenuStyle.hpp"
#include "Misc/Style.hpp"

void Menu::addMenuItem(const MenuItem &newItem) {
    menuItems_.push_back(newItem);
    if (newItem.menuItemType == MenuItem::MenuItemType::NORMAL) {
        numMenuItems_++;
    }
}

void Menu::resetMenuItems() {
    menuItems_.clear();
    numMenuItems_ = 0;
    currSelection_ = 0;
    currMenuPage_ = 0;
}

void Menu::populateMenuItems(std::vector<MenuItem> &menuItems) {
    int itemCount = 0;
    for (int i = 0; i < menuItems_.size() && itemCount < myStyle_.numberOfItemsPerPage; i++) {
        if (i >= currMenuPage_ * myStyle_.numberOfItemsPerPage) {
            menuItems.push_back(menuItems_[i]);
            if (menuItems_[i].menuItemType == MenuItem::MenuItemType::NORMAL) {
                itemCount++;
            }
        }
    }
}

auto Menu::getNumItems() const -> int { return numMenuItems_; }

auto Menu::getMenuItemByTitle(const std::string &title) -> int {
    int normalItemCount = 0;
    for (int i = 0; i < menuItems_.size(); i++) {
        if (menuItems_[i].menuItemType == MenuItem::MenuItemType::NORMAL) {
            if (menuItems_[i].title == title) {
                return normalItemCount;
            }
            normalItemCount++;
        }
    }
    return -1;
}

void Menu::setCurrSelection(int newSelection) {
    currSelection_ = newSelection;
    if (myStyle_.numberOfItemsPerPage == 0) {
        currMenuPage_ = 0;
    } else {
        int normalItemCount = 0;
        for (int i = 0; i < menuItems_.size(); i++) {
            if (menuItems_[i].menuItemType == MenuItem::MenuItemType::NORMAL) {
                if (normalItemCount == currSelection_) {
                    currMenuPage_ = i / myStyle_.numberOfItemsPerPage;
                    break;
                }
                normalItemCount++;
            }
        }
    }
}

auto Menu::getCurrSelection() const -> int { return currSelection_; }

auto Menu::getCurrItem() -> MenuItem {
    int normalItemCount = 0;
    for (auto &item : menuItems_) {
        if (item.menuItemType == MenuItem::MenuItemType::NORMAL) {
            if (normalItemCount == currSelection_) {
                return item;
            }
            normalItemCount++;
        }
    }
    // This should never happen if the menu is properly set up
    return MenuItem();
}

auto Menu::updateCurrItemTitle(std::string newTitle) -> bool {
    int normalItemCount = 0;
    for (auto &item : menuItems_) {
        if (item.menuItemType == MenuItem::MenuItemType::NORMAL) {
            if (normalItemCount == currSelection_) {
                if (item.title != newTitle) {
                    item.title = std::move(newTitle);
                    return true;
                }
                return false;
            }
            normalItemCount++;
        }
    }
    return false;
}

auto Menu::nextSelection(bool skipLast) -> bool {
    bool needsRefresh = false;
    int oldCurMenuPage = currMenuPage_;

    do {
        currSelection_ = (currSelection_ + 1) % numMenuItems_;
    } while (menuItems_[currSelection_].menuItemType != MenuItem::MenuItemType::NORMAL);

    if (skipLast && (currSelection_ == numMenuItems_ - 1)) {
        currSelection_ = 0;
    }

    if (myStyle_.numberOfItemsPerPage == 0) {
        currMenuPage_ = 0;
    } else {
        int normalItemCount = 0;
        for (int i = 0; i < menuItems_.size(); i++) {
            if (menuItems_[i].menuItemType == MenuItem::MenuItemType::NORMAL) {
                if (normalItemCount == currSelection_) {
                    currMenuPage_ = i / myStyle_.numberOfItemsPerPage;
                    break;
                }
                normalItemCount++;
            }
        }
    }

    if (oldCurMenuPage != currMenuPage_) {
        log_d("Page changed, refreshing");
        needsRefresh = true;
    }

    return needsRefresh;
}

auto Menu::prevSelection(bool skipLast) -> bool {
    bool needsRefresh = false;
    int oldCurMenuPage = currMenuPage_;

    do {
        currSelection_ = (currSelection_ + numMenuItems_ - 1) % numMenuItems_;
    } while (menuItems_[currSelection_].menuItemType != MenuItem::MenuItemType::NORMAL);

    if (skipLast && (currSelection_ == numMenuItems_ - 1)) {
        currSelection_--;
    }

    if (myStyle_.numberOfItemsPerPage == 0) {
        currMenuPage_ = 0;
    } else {
        int normalItemCount = 0;
        for (int i = 0; i < menuItems_.size(); i++) {
            if (menuItems_[i].menuItemType == MenuItem::MenuItemType::NORMAL) {
                if (normalItemCount == currSelection_) {
                    currMenuPage_ = i / myStyle_.numberOfItemsPerPage;
                    break;
                }
                normalItemCount++;
            }
        }
    }

    if (oldCurMenuPage != currMenuPage_) {
        needsRefresh = true;
    }

    return needsRefresh;
}

auto Menu::getItemTitleAt(int index) -> std::string {
    int normalItemCount = 0;
    for (auto &item : menuItems_) {
        if (item.menuItemType == MenuItem::MenuItemType::NORMAL) {
            if (normalItemCount == index) {
                return item.title;
            }
            normalItemCount++;
        }
    }
    return "";
}

void Menu::changeItemTitleAt(int index, std::string title) {
    int normalItemCount = 0;
    for (auto &item : menuItems_) {
        if (item.menuItemType == MenuItem::MenuItemType::NORMAL) {
            if (normalItemCount == index) {
                item.title = std::move(title);
                return;
            }
            normalItemCount++;
        }
    }
}

auto Menu::getItemAt(int index) -> MenuItem {
    int normalItemCount = 0;
    for (auto &item : menuItems_) {
        if (item.menuItemType == MenuItem::MenuItemType::NORMAL) {
            if (normalItemCount == index) {
                return item;
            }
            normalItemCount++;
        }
    }
    // This should never happen if the menu is properly set up
    return MenuItem();
}
