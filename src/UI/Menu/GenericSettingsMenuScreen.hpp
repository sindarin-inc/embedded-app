#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"

class GenericSettingsMenuScreen : public Screen, public EinkView {
public:
    GenericSettingsMenuScreen(DisplaySystem &display, const std::string &backTitle);

    auto setSelectedIndex(size_t index) -> bool;

    void addMenuItem(const std::shared_ptr<MenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void render(DisplaySystem &display) override;

private:
    MenuView menuView_;
    ArrowLabelView backLabelView_;

    void moveSelection(int delta);
};
