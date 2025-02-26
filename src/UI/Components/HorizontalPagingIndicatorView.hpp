#pragma once

#include "UI/Components/EinkView.hpp"

class HorizontalPagingIndicatorView : public EinkView {
public:
    HorizontalPagingIndicatorView(DisplaySystem &display) : EinkView(display){};

    void setTotalPages(int totalPages);
    auto totalPages() const { return totalPages_; }
    void setCurrentPage(int currentPage);

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    int indicatorSpacing = 5;
    int indicatorSize = 4;

private:
    int totalPages_ = 1;
    int currentPage_ = 0;
};
