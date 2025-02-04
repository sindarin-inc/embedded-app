#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Platform.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"

class InactivitySettingMenuScreen : public Screen, public EinkView {
public:
    InactivitySettingMenuScreen(DisplaySystem &display, const std::string &backTitle);

    auto setSelectedIndex(size_t index) -> bool;

    void addMenuItem(const std::shared_ptr<MenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void render(DisplaySystem &display) override;

    void setExtraText(const std::string &text) { extraTextView_.setText(text); };

    void requireRestart() { restartOnBack_ = true; }

    auto getRequireRestart() const -> bool { return restartOnBack_; }

private:
    MenuView menuView_;
    ArrowLabelView backLabelView_;
    SingleLineTextView extraTextView_;

    void moveSelection(int delta);

    bool restartOnBack_{false};
};
