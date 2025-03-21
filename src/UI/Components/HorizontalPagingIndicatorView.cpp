#include "HorizontalPagingIndicatorView.hpp"

#include "sindarin-debug.h"

void HorizontalPagingIndicatorView::setTotalPages(int totalPages) {
    totalPages_ = std::max(1, totalPages);
    setNeedsLayout();
}

void HorizontalPagingIndicatorView::setCurrentPage(int currentPage) {
    currentPage_ = std::clamp(currentPage, 0, totalPages_ - 1);
    setNeedsLayout();
}

auto HorizontalPagingIndicatorView::layout(LayoutContext &context, SolSize size) -> SolSize {
    int totalWidth = totalPages_ * indicatorSize + (totalPages_ - 1) * indicatorSpacing;
    return {totalWidth, indicatorSize};
}

void HorizontalPagingIndicatorView::drawInRect(SolRect rect) {
    int y = rect.origin.y;
    int startX = rect.origin.x;

    for (int i = 0; i < totalPages_; ++i) {
        int x = startX + i * (indicatorSize + indicatorSpacing);
        if (i == currentPage_) {
            display_.fillCircle(x + indicatorSize / 2, y + indicatorSize / 2, indicatorSize / 2,
                                display_.fg);
        } else {
            display_.drawCircle(x + indicatorSize / 2, y + indicatorSize / 2, indicatorSize / 2,
                                display_.fg);
        }
    }
}
