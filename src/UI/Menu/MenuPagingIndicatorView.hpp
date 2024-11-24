#pragma once

#include "UI/Components/EinkView.hpp"

class MenuPagingIndicatorView : public EinkView {
public:
    MenuPagingIndicatorView(DisplaySystem &display) : EinkView(display){};

    void setTotalPages(int totalPages);
    auto totalPages() const { return totalPages_; }
    void setCurrentPage(int currentPage);

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    int indicatorSpacing = 5;

private:
    int totalPages_ = 1;
    int currentPage_ = 0;
};
