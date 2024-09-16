#pragma once

#include <vector>

#include "NewMenuItem.hpp"
#include "NewMenuItemView.hpp"
#include "NewMenuPagingIndicatorView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"

class NewMenuView : public EinkView {
public:
    enum class IndicatorType { Dot, Line, None };

    NewMenuView(DisplaySystem &display);

    void addMenuItem(const std::shared_ptr<NewMenuItem> &menuItem);
    auto setSelectedIndex(size_t index) -> bool;
    auto moveSelection(int delta) -> bool;
    void setIndicatorType(IndicatorType type);

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    auto getSelectedIndex() const -> size_t { return selectedIndex_; }
    auto getItemCount() const -> size_t { return menuItems_.size(); }
    void executeSelectedItem() {
        if (selectedIndex_ < menuItems_.size()) {
            menuItems_[selectedIndex_]->execute();
        }
    }

    void drawIndicator() {
        indicatorView_.fillColor = display_.fg;
        indicatorView_.strokeColor = display_.fg;
        if (indicatorType_ != IndicatorType::None) {
            indicatorView_.draw();
        }
    }

    uint16_t padding = 5;
    uint8_t itemsPerPage = 8;
    uint16_t indicatorWidth = 0;         // Default in constructor
    uint16_t indicatorPadding = 0;       // Default in constructor
    uint16_t pagingIndicatorWidth = 0;   // Default in constructor
    uint16_t pagingIndicatorPadding = 0; // Default in constructor

private:
    std::vector<std::shared_ptr<NewMenuItem>> menuItems_;
    size_t selectedIndex_ = 0;
    size_t currentPage_ = 0;
    IndicatorType indicatorType_ = IndicatorType::Dot;

    std::vector<std::shared_ptr<NewMenuItemView>> menuItemViews_;
    NewMenuPagingIndicatorView pagingIndicatorView_;
    BoxView indicatorView_;

    void createMenuItemViews();
};
