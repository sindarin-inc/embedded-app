#include "NewMenuView.hpp"

#include "NewMenuItemSingleLineView.hpp"

NewMenuView::NewMenuView(DisplaySystem &display)
    : EinkView(display),
      // 5px on 256x256, scales up for larger displays
      indicatorWidth(std::max(5, std::min(display.width(), display.height()) / 51)),
      indicatorPadding(indicatorWidth), pagingIndicatorWidth(indicatorWidth),
      pagingIndicatorPadding(2 * indicatorWidth), pagingIndicatorView_(display),
      indicatorView_(display, 0) {}

void NewMenuView::addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem) {
    menuItems_.push_back(menuItem);
    createMenuItemViews();

    int totalPages = (menuItems_.size() + itemsPerPage - 1) / itemsPerPage;
    if (totalPages > 1) {
        pagingIndicatorView_.setTotalPages(totalPages);
        pagingIndicatorView_.setHidden(false);
    } else {
        pagingIndicatorView_.setHidden(true);
    }
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

void NewMenuView::createMenuItemViews() {
    menuItemViews_.clear();
    for (const auto &menuItem : menuItems_) {
        // NOTE: Here is where you create different item views based on the menu item models
        if (auto item = std::dynamic_pointer_cast<NewMenuItemSingleLine>(menuItem)) {
            menuItemViews_.push_back(std::make_unique<NewMenuItemSingleLineView>(display_, item));
        }
    }
}

auto NewMenuView::layout(LayoutContext &context, Size size) -> Size {
    int y = 0;
    int x = 0;

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
    }

    // The remaining space for the menu items

    // Draw the menu items
    size_t startIndex = currentPage_ * itemsPerPage;
    size_t endIndex = std::min(startIndex + itemsPerPage, menuItemViews_.size());
    for (size_t i = startIndex; i < endIndex; ++i) {
        bool selected = i == selectedIndex_;
        (*menuItemViews_[i]).setSelected(selected);
        auto itemRect = context.setViewFrame(*menuItemViews_[i], {{x, y}, {width, 0}},
                                             LayoutOptionsSizeToFit | LayoutOptionsConstrainWidth);
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

    return {size.width, y};
}

void NewMenuView::drawInRect(Rect rect) {
    size_t startIndex = currentPage_ * itemsPerPage;
    size_t endIndex = std::min(startIndex + itemsPerPage, menuItemViews_.size());

    for (size_t i = startIndex; i < endIndex; ++i) {
        menuItemViews_[i]->draw();
    }

    pagingIndicatorView_.draw();
}
