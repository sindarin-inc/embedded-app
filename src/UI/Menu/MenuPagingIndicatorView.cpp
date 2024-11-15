#include "MenuPagingIndicatorView.hpp"

void MenuPagingIndicatorView::setTotalPages(int totalPages) {
    totalPages_ = std::max(1, totalPages);
    setNeedsLayout();
}

void MenuPagingIndicatorView::setCurrentPage(int currentPage) {
    currentPage_ = std::clamp(currentPage, 0, totalPages_ - 1);
    setNeedsLayout();
}

auto MenuPagingIndicatorView::layout(LayoutContext &context, Size size) -> Size {
    int totalHeight = totalPages_ * size.width + (totalPages_ - 1) * indicatorSpacing;
    return {size.width, totalHeight};
}

void MenuPagingIndicatorView::drawInRect(Rect rect) {
    int x = rect.origin.x;
    int startY = rect.origin.y;
    int indicatorWidth = rect.size.width;

    for (int i = 0; i < totalPages_; ++i) {
        int y = startY + i * (indicatorWidth + indicatorSpacing);
        if (i == currentPage_) {
            display_.fillRoundRect(x, y, indicatorWidth, indicatorWidth, indicatorWidth * 5 / 3,
                                   display_.fg);
        } else {
            display_.drawRoundRect(x, y, indicatorWidth, indicatorWidth, indicatorWidth * 5 / 3,
                                   display_.fg);
        }
    }
}