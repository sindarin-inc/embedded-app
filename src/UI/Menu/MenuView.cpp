#include "MenuView.hpp"

#include "DarkModeMenuItemCheckableView.hpp"
#include "MenuItemArticleView.hpp"
#include "MenuItemBookView.hpp"
#include "MenuItemCheckableView.hpp"
#include "MenuItemMultiLineView.hpp"
#include "MenuItemSingleLineView.hpp"
#include "MenuItemTOCView.hpp"

MenuView::MenuView(DisplaySystem &display)
    : EinkView(display),
      // 5px on 256x256, scales up for larger displays
      indicatorWidth(std::max(5, std::min(display.width(), display.height()) / 51)),
      indicatorPadding(indicatorWidth), pagingIndicatorWidth(indicatorWidth),
      pagingIndicatorPadding(2 * indicatorWidth), pagingIndicatorView_(display),
      indicatorView_(display, 0) {}

void MenuView::addMenuItem(const std::shared_ptr<MenuItem> &menuItem) {
    menuItems_.push_back(menuItem);

    // NOTE: Here is where you create different item views based on the menu item models
    if (auto item = std::dynamic_pointer_cast<MenuItemSingleLine>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<MenuItemSingleLineView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<MenuItemMultiLine>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<MenuItemMultiLineView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<MenuItemBook>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<MenuItemBookView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<MenuItemArticle>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<MenuItemArticleView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<DarkModeMenuItemCheckable>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<DarkModeMenuItemCheckableView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<MenuItemCheckable>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<MenuItemCheckableView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<MenuItemTOC>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<MenuItemTOCView>(display_, item));
    }

    int totalPages = (menuItems_.size() + itemsPerPage - 1) / itemsPerPage;
    if (totalPages > 1) {
        pagingIndicatorView_.setTotalPages(totalPages);
        pagingIndicatorView_.setHidden(false);
    } else {
        pagingIndicatorView_.setHidden(true);
    }

    setNeedsLayout();
}

auto MenuView::setSelectedIndex(size_t index) -> bool {
    if (index < menuItems_.size()) {
        selectedIndex_ = index % menuItems_.size();
        setNeedsLayout();
        auto page = selectedIndex_ / itemsPerPage;
        if (page != currentPage_) {
            currentPage_ = page;
            pagingIndicatorView_.setCurrentPage(currentPage_);
            return true;
        }
    }
    return false;
}

auto MenuView::setSelectedByMenuItemID(const std::string &menuItemID) -> bool {
    for (size_t i = 0; i < menuItems_.size(); ++i) {
        if (menuItems_[i]->id() == menuItemID) {
            return setSelectedIndex(i);
        }
    }
    // If we don't find it, select the first item.
    return setSelectedIndex(0);
}

auto MenuView::getIDfromRowIndex(size_t index) const -> std::string {
    if (index < menuItems_.size()) {
        return menuItems_[index]->id();
    }
    return "";
}

auto MenuView::getSelectedMenuItemID() const -> std::string {
    if (selectedIndex_ < menuItems_.size()) {
        return menuItems_[selectedIndex_]->id();
    }
    return "";
}

auto MenuView::isIDInMenu(const std::string &id) -> bool {
    for (auto &menuItem : menuItems_) {
        if (menuItem->id() == id) {
            return true;
        }
    }
    return false;
}

auto MenuView::moveSelection(int delta) -> bool {
    int newIndex = static_cast<int>(getSelectedIndex()) + delta;
    // Wrap around
    if (newIndex < 0) {
        newIndex = getItemCount() - 1;
    } else if (newIndex >= getItemCount()) {
        newIndex = newIndex % getItemCount();
    }
    return setSelectedIndex(newIndex);
}

void MenuView::setIndicatorType(IndicatorType type) {
    if (indicatorType_ != type) {
        indicatorType_ = type;
        setNeedsLayout();
    }
}

auto MenuView::layout(LayoutContext &context, SolSize size) -> SolSize {
    int y = 0;
    int x = 0;
    int widthOfPagingIndicators = 0;

    // Reserve space for indicator
    if (indicatorType_ != IndicatorType::None) {
        x += indicatorWidth + indicatorPadding;
    }
    int width = size.width - x;

    auto pagingIndicatorRect = SolRect::zero();
    // Layout paging indicator on the right
    if (!pagingIndicatorView_.isHidden()) {
        pagingIndicatorRect = context.setViewSizeInRect(
            pagingIndicatorView_, {pagingIndicatorWidth, 0}, SolRect::fromSize(size),
            LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical |
                LayoutOptionsAlignRight);
        width -= pagingIndicatorRect.size.width + pagingIndicatorPadding;
        widthOfPagingIndicators += pagingIndicatorRect.size.width + indicatorPadding;
    }

    // The remaining space for the menu items
    // Draw the menu items
    int maxWidth = 0;
    size_t startIndex = currentPage_ * itemsPerPage;
    size_t endIndex = std::min(startIndex + itemsPerPage, menuItemViews_.size());
    for (size_t i = startIndex; i < endIndex; ++i) {
        bool selected = i == selectedIndex_;
        (*menuItemViews_[i]).setSelected(selected);
        auto itemRect = context.setViewFrame(*menuItemViews_[i], {{x, y}, {width, 0}},
                                             LayoutOptionsSizeToFit | LayoutOptionsConstrainWidth);
        maxWidth = std::max(maxWidth, itemRect.right());
        y += itemRect.size.height + padding;

        // If this is the selected item, draw the indicator center aligned with the item
        if (selected && indicatorType_ != IndicatorType::None) {
            // 1 px nudge downards to account for the fact that the center of gravity of text is
            // a little lower than its center
            SolRect indicatorRect = {{0, itemRect.origin.y + 1},
                                     {indicatorWidth, itemRect.size.height}};
            SolSize indicatorSize{};
            switch (indicatorType_) {
            case IndicatorType::None:
            case IndicatorType::Dot:
                indicatorSize = {indicatorWidth, indicatorWidth};
                break;
            case IndicatorType::Line:
                indicatorSize = {indicatorWidth, itemRect.size.height + 4};
                break;
            }
            indicatorView_.cornerRadius = indicatorWidth / 2;
            context.setViewSizeInRect(indicatorView_, indicatorSize, indicatorRect,
                                      LayoutOptionsAlignCenterVertical);
        }
    }
    y -= padding;

    return {maxWidth + widthOfPagingIndicators, y};
}

void MenuView::drawInRect(SolRect rect) {
    size_t startIndex = currentPage_ * itemsPerPage;
    size_t endIndex = std::min(startIndex + itemsPerPage, menuItemViews_.size());

    for (size_t i = startIndex; i < endIndex; ++i) {
        menuItemViews_[i]->draw();
    }

    pagingIndicatorView_.draw();
}
