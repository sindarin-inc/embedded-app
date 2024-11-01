#include "NewMenuView.hpp"

#include "NewMenuItemArticleView.hpp"
#include "NewMenuItemBookView.hpp"
#include "NewMenuItemCheckableView.hpp"
#include "NewMenuItemMultiLineView.hpp"
#include "NewMenuItemSingleLineView.hpp"
#include "NewMenuItemTOCView.hpp"

NewMenuView::NewMenuView(DisplaySystem &display)
    : EinkView(display),
      // 5px on 256x256, scales up for larger displays
      indicatorWidth(std::max(5, std::min(display.width(), display.height()) / 51)),
      indicatorPadding(indicatorWidth), pagingIndicatorWidth(indicatorWidth),
      pagingIndicatorPadding(2 * indicatorWidth), pagingIndicatorView_(display),
      indicatorView_(display, 0) {}

void NewMenuView::addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem) {
    menuItems_.push_back(menuItem);

    // NOTE: Here is where you create different item views based on the menu item models
    if (auto item = std::dynamic_pointer_cast<NewMenuItemSingleLine>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<NewMenuItemSingleLineView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<NewMenuItemMultiLine>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<NewMenuItemMultiLineView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<NewMenuItemBook>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<NewMenuItemBookView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<NewMenuItemArticle>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<NewMenuItemArticleView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<NewMenuItemCheckable>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<NewMenuItemCheckableView>(display_, item));
    }
    if (auto item = std::dynamic_pointer_cast<NewMenuItemTOC>(menuItem)) {
        menuItemViews_.push_back(std::make_unique<NewMenuItemTOCView>(display_, item));
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

auto NewMenuView::setSelectedIndex(size_t index) -> bool {
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

auto NewMenuView::setSelectedByMenuItemID(const std::string &menuItemID) -> bool {
    for (size_t i = 0; i < menuItems_.size(); ++i) {
        if (menuItems_[i]->id() == menuItemID) {
            return setSelectedIndex(i);
        }
    }
    // If we don't find it, select the first item.
    return setSelectedIndex(0);
}

auto NewMenuView::getIDfromRowIndex(size_t index) const -> std::string {
    if (index < menuItems_.size()) {
        return menuItems_[index]->id();
    }
    return "";
}

auto NewMenuView::getSelectedMenuItemID() const -> std::string {
    if (selectedIndex_ < menuItems_.size()) {
        return menuItems_[selectedIndex_]->id();
    }
    return "";
}

auto NewMenuView::isIDInMenu(const std::string &id) -> bool {
    for (auto &menuItem : menuItems_) {
        if (menuItem->id() == id) {
            return true;
        }
    }
    return false;
}

auto NewMenuView::moveSelection(int delta) -> bool {
    int newIndex = static_cast<int>(getSelectedIndex()) + delta;
    // Wrap around
    if (newIndex < 0) {
        newIndex = getItemCount() - 1;
    } else if (newIndex >= getItemCount()) {
        newIndex = newIndex % getItemCount();
    }
    return setSelectedIndex(newIndex);
}

void NewMenuView::setIndicatorType(IndicatorType type) {
    if (indicatorType_ != type) {
        indicatorType_ = type;
        setNeedsLayout();
    }
}

auto NewMenuView::layout(LayoutContext &context, Size size) -> Size {
    int y = 0;
    int x = 0;
    int widthOfPagingIndicators = 0;

    // Reserve space for indicator
    if (indicatorType_ != IndicatorType::None) {
        x += indicatorWidth + indicatorPadding;
    }
    int width = size.width - x;

    auto pagingIndicatorRect = Rect::zero();
    // Layout paging indicator on the right
    if (!pagingIndicatorView_.isHidden()) {
        pagingIndicatorRect = context.setViewSizeInRect(
            pagingIndicatorView_, {pagingIndicatorWidth, 0}, Rect::fromSize(size),
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
            Rect indicatorRect = {{0, itemRect.origin.y + 1},
                                  {indicatorWidth, itemRect.size.height}};
            Size indicatorSize{};
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

void NewMenuView::drawInRect(Rect rect) {
    size_t startIndex = currentPage_ * itemsPerPage;
    size_t endIndex = std::min(startIndex + itemsPerPage, menuItemViews_.size());

    for (size_t i = startIndex; i < endIndex; ++i) {
        menuItemViews_[i]->draw();
    }

    pagingIndicatorView_.draw();
}
