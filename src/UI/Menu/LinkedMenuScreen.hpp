#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/RendererView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/NewMenuItemSingleLine.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/RootUI.hpp"
#include "UI/Screen.hpp"

extern RootUI rootUi;

// The purpose of this screen type is to allow linked lists of screens and
// this handles the general navigation logic. Subclasses will handle the specifics
// of layout. This is similar to the CarouselScreen in terms of abstraction puropse.
class LinkedMenuScreen : public Screen, public EinkView {
public:
    LinkedMenuScreen(DisplaySystem &display, std::string backLabel);

    void setNextScreen(const std::shared_ptr<Screen> screen, std::string title) {
        nextScreen_ = screen;
        addMenuItem(std::make_shared<NewMenuItemSingleLine>(title, [this]() {
            if (nextScreen_) {
                gotoNextScreen();
            }
        }));
    }
    void setPreviousScreen(const std::shared_ptr<Screen> screen, std::string title) {
        previousScreen_ = screen;
        addMenuItem(std::make_shared<NewMenuItemSingleLine>(title, [this]() {
            if (previousScreen_) {
                gotoPreviousScreen();
            }
        }));
    }

    void addBackMenuItem() {
        addMenuItem(std::make_shared<NewMenuItemSingleLine>(backLabel_, [this]() { back(); }));
    }

    auto getNextScreen() -> std::shared_ptr<Screen> { return nextScreen_; }
    auto getPreviousScreen() -> std::shared_ptr<Screen> { return previousScreen_; }

    void addMenuItem(const std::shared_ptr<NewMenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;
    void render(DisplaySystem &display) override;

protected:
    void gotoNextScreen() {
        rootUi.popScreen();
        rootUi.pushScreen(nextScreen_);
    }

    void gotoPreviousScreen() {
        rootUi.popScreen();
        rootUi.pushScreen(previousScreen_);
    };

private:
    std::shared_ptr<Screen> nextScreen_{nullptr};
    std::shared_ptr<Screen> previousScreen_{nullptr};

    NewMenuView menuView_;
    SingleLineTextView backLabelView_;
    std::string backLabel_;

    MultiLineTextView rendererView_;

    void moveSelection(int delta);
};
