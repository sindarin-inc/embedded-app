#pragma once

#include <Renderer/Renderer.hpp>

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/RendererView.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/RootUI.hpp"
#include "UI/Screen.hpp"

class LetsFocusWelcomeMenuScreen : public Screen, public EinkView {
public:
    LetsFocusWelcomeMenuScreen(DisplaySystem &display, PreferencesStore &prefs);

    void addMenuItem(const std::shared_ptr<MenuItem> &item);
    void processCommand(RemoteCommand command) override;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;
    void render(DisplaySystem &display) override;

private:
    PreferencesStore &prefs_;
    MenuView menuView_;
    Renderer renderer_;
    RendererView rendererView_;
    ArrowLabelView leftArrowView_;
    ArrowLabelView rightArrowView_;
    uint8_t textSelection_ = 0;

    std::vector<std::tuple<std::string, SolInset>> texts_ = {
        {"Twist each lens at the ridged edge like binoculars until this text is in focus.",
         {50, 42, 77, 53}},
        {"For some people, the distance between their eyes impacts focus. The knob on top of your "
         "Sol Reader allows you to adjust the eye spacing. Line the lenses up so each one is "
         "directly in front of each eye.",
         {35, 21, 45, 34}},
        {"Feeling good? Press \ue000 to continue your setup!\n\n"
         "Also, you just learned how to turn the page  \ue002  and go back a page  \ue001 . "
         "Well done! Fun fact: while reading a book, you can also use the \ue000 button to turn "
         "the page.",
         {35, 21, 45, 34}},
    };

    void setText();
    void moveSelection(int delta);
};
