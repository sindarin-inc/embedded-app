#pragma once

#include "UI/Components/EinkView.hpp"

class MenuPagingIndicatorView : public EinkView {
public:
    MenuPagingIndicatorView(DisplaySystem &display) : EinkView(display){};

    void setTotalPages(int totalPages);
    auto totalPages() const { return totalPages_; }
    void setCurrentPage(int currentPage);

    auto layout(SolUI::LayoutContext &context, SolUI::Size size) -> SolUI::Size override;
    void drawInRect(SolUI::Rect rect) override;

    int indicatorSpacing = 5;

private:
    int totalPages_ = 1;
    int currentPage_ = 0;
};
