#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/Screen.hpp"

class NewMenuScreen : public Screen, public EinkView {
public:
    NewMenuScreen(DisplaySystem &display, std::string title);

    void addMenuItem(const std::shared_ptr<NewMenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    // Implement Screen methods
    void render(DisplaySystem &display) override;

private:
    NewMenuView menuView_;
    SingleLineTextView titleView_;

    bool firstLoad_{true};
    std::string title_;

    void moveSelection(int delta);
};
