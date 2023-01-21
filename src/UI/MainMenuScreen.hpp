#ifndef MAIN_MENU_SCREEN_H
#define MAIN_MENU_SCREEN_H

#include "Misc/MenuStyle.hpp"
#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/MenuItemView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/StackView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/MenuScreen.hpp"
#include "sindarin-debug.h"

class MainMenuScreen : public MenuScreen, public EinkView {
public:
    MainMenuScreen(Power &power, DisplaySystem &display, MenuStyleFamily myStyle)
        : MenuScreen(power, myStyle), EinkView(display) {
#ifdef SWAP_ARTICLES_BOOKS
        numArticleItems_ = 1;
        MenuItem dummy("DUMMY", []() {});
        articleItems_.push_back(dummy);
#endif
        upperLeftText_ = "--";
        lowerRightText_ = "--";
    };

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;

    void addSubMenuScreen(MenuScreen *newSubScreen);

protected:
    void processCommand(RemoteCommand command) override;

    std::vector<MenuScreen *> subMenus_;

    std::vector<std::shared_ptr<MenuItemView>> menuItemViews_;
    std::vector<std::shared_ptr<BoxView>> menuPageViews_;

    std::vector<std::shared_ptr<SingleLineTextView>> topBarViews_;
    std::shared_ptr<BoxView> topBarSelectionView_;

    std::shared_ptr<IconView> batteryIconView_;
    std::shared_ptr<SingleLineTextView> batteryTextView_;
    std::shared_ptr<SingleLineTextView> clockView_;

    int currentSelection_ = 0;

#ifdef SWAP_ARTICLES_BOOKS
    // The menu items for long-form books come from the parent class. Since the Main Menu
    // essentially keeps two lists, we need a second set of items for the articles.
    // There is likely a more elegant way to do this.
    int numArticleItems_ = 0;
    std::vector<MenuItem> articleItems_;
    int currArticleSelection_ = 0;
    int currArticlePage_ = 0;

    int numBookItems_ = 0;
    std::vector<MenuItem> bookItems_;
    int currBookSelection_ = 0;
    int currBookPage_ = 0;

    bool inBooksMode_ = true;

    bool beforeFirstLoad_ = true;
#endif
};

#endif // MAIN_MENU_SCREEN_H