#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/Screen.hpp"

class SystemSettingsMenuScreen : public Screen, public EinkView {
public:
    SystemSettingsMenuScreen(DisplaySystem &display, const std::string &backTitle);

    auto setSelectedIndex(size_t index) -> bool;

    void addMenuItem(const std::shared_ptr<NewMenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void setFirstLoad() { firstLoad_ = true; }

    void showTemporaryMessage(DisplaySystem &display, const std::string &message,
                              int duration = 2000, bool rerenderAfter = false);

    void render(DisplaySystem &display) override;

private:
    NewMenuView menuView_;
    ArrowLabelView backLabelView_;

    bool firstLoad_{true};

    void moveSelection(int delta);
};
