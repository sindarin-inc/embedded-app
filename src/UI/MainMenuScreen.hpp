#ifndef MAIN_MENU_SCREEN_H
#define MAIN_MENU_SCREEN_H

#include "Misc/MenuStyle.hpp"
#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/MenuScreen.hpp"
#include "sindarin-debug.h"

class MainMenuScreen : public MenuScreen {

public:
    MainMenuScreen(Power &power, MenuStyleFamily myStyle) : MenuScreen(power, myStyle) {
#ifdef SWAP_ARTICLES_BOOKS
        numArticleItems_ = 1;
        MenuItem dummy("DUMMY", []() {});
        articleItems_.push_back(dummy);
#endif
        upperLeftText_ = "Settings";
        lowerRightText_ = "Read";
    };

    void render(DisplaySystem &display) override;

protected:
    void processCommand(RemoteCommand command) override;

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