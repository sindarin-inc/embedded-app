#pragma once

#include <memory>
#include <vector>

#include "Models/Readable.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/StackView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class LineMenuScreen : public Screen, public EinkView {
public:
    LineMenuScreen(PreferencesStore &prefs, DisplaySystem &display)
        : EinkView(display), menuView_(display), display_(display) {
        menuView_.padding = 3;
        menuView_.itemsPerPage = 4;
        menuView_.indicatorWidth = 5;
        menuView_.indicatorPadding = 5;
        menuView_.setIndicatorType(MenuView::IndicatorType::Line);
    }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    void resetMenuItems() {
        menuView_.resetMenuItems();
        firstLoad_ = true;
    }

    void addMenuItem(const std::shared_ptr<MenuItem> &item) { menuView_.addMenuItem(item); }

protected:
    MenuView menuView_;
    DisplaySystem &display_;
    bool firstLoad_ = true;

    void moveSelection(int delta);
};
