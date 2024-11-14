#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"

class MenuScreen : public Screen, public EinkView {
public:
    MenuScreen(DisplaySystem &display, std::string title);

    void addMenuItem(const std::shared_ptr<MenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    // Implement Screen methods
    void render(DisplaySystem &display) override;

private:
    MenuView menuView_;
    SingleLineTextView titleView_;

    bool firstLoad_{true};
    std::string title_;

    void moveSelection(int delta);
};
