#pragma once

#include <vector>

#include "MenuItem.hpp"
#include "MenuItemView.hpp"
#include "MenuPagingIndicatorView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"

class MenuView : public EinkView {
public:
    enum class IndicatorType { Dot, Line, None };

    MenuView(DisplaySystem &display);

    void addMenuItem(const std::shared_ptr<MenuItem> &menuItem);
    void resetMenuItems() {
        menuItems_.clear();
        menuItemViews_.clear();
        selectedIndex_ = 0;
        currentPage_ = 0;
    }
    auto setSelectedIndex(size_t index) -> bool;
    auto moveSelection(int delta) -> bool;
    void setIndicatorType(IndicatorType type);

    auto setSelectedByMenuItemID(const std::string &menuItemID) -> bool;
    auto getSelectedMenuItemID() const -> std::string;

    auto getIDfromRowIndex(size_t index) const -> std::string;

    auto isIDInMenu(const std::string &id) -> bool;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    auto getSelectedIndex() const -> size_t { return selectedIndex_; }
    auto getItemCount() const -> size_t { return menuItems_.size(); }
    void executeSelectedItem() {
        if (selectedIndex_ < menuItems_.size()) {
            menuItems_[selectedIndex_]->execute();
        }
    }
    void executeSelectedItemRightButton() {
        if (selectedIndex_ < menuItems_.size()) {
            menuItems_[selectedIndex_]->executeRightButton();
        }
    }

    void drawFastElements() {
        indicatorView_.fillColor = display_.fg;
        indicatorView_.strokeColor = display_.fg;
        if (indicatorType_ != IndicatorType::None) {
            indicatorView_.draw();
        }

        size_t startIndex = currentPage_ * itemsPerPage;
        size_t endIndex = std::min(startIndex + itemsPerPage, menuItemViews_.size());

        for (size_t i = startIndex; i < endIndex; ++i) {
            menuItemViews_[i]->drawFastOnlyElements();
        }
    }

    uint16_t padding = 5;
    uint8_t itemsPerPage = 8;
    uint16_t indicatorWidth = 0;         // Default in constructor
    uint16_t indicatorPadding = 0;       // Default in constructor
    uint16_t pagingIndicatorWidth = 0;   // Default in constructor
    uint16_t pagingIndicatorPadding = 0; // Default in constructor

private:
    std::vector<std::shared_ptr<MenuItem>> menuItems_;
    size_t selectedIndex_ = 0;
    size_t currentPage_ = 0;
    IndicatorType indicatorType_ = IndicatorType::Dot;

    std::vector<std::shared_ptr<MenuItemView>> menuItemViews_;
    MenuPagingIndicatorView pagingIndicatorView_;
    BoxView indicatorView_;

    void createMenuItemViews();
};
