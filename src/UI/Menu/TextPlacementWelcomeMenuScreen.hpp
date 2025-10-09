#pragma once

#include <Renderer/Renderer.hpp>

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/RendererView.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/RootUI.hpp"
#include "UI/Screen.hpp"

class TextPlacementWelcomeMenuScreen : public Screen, public EinkView {
public:
    TextPlacementWelcomeMenuScreen(DisplaySystem &display);

    void addMenuItem(const std::shared_ptr<MenuItem> &item);
    void processCommand(RemoteCommand command) override;

    [[nodiscard]] auto getCurrentTextSelection() const -> uint8_t { return textSelection_; }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;
    void render(DisplaySystem &display) override;

private:
    MenuView menuView_;
    Renderer renderer_;
    RendererView rendererView_;
    ArrowView leftArrowView_;
    ArrowView rightArrowView_;
    uint8_t textSelection_ = 0;

    std::vector<std::tuple<std::string, SolInset>> texts_ = {
        {"These screens will show you three template views for text placement. You can select your "
         "preferred text placement now; don't worry, it can be changed anytime and customized even "
         "further in the Settings menu!\n\n\n"
         "This is an example of the Comfortable template, which is the default.\n\nIt features "
         "small text, average line spacing, and large margins.",
         {0, 0, 0, 0}},
        {"This is the Condensed template for those who like to pack as many words on the page as "
         "possible. The text has the smallest line spacing and takes up the most space on the "
         "screen.\n\nCan you believe how much additional, seemingly unnecessary filler text we're "
         "being forced to write here just for the sole purpose of making sure the content "
         "stretches out to fill up the page, even though it doesn't really add any value or "
         "substance to what we're trying to communicate in the first place?\n\nIt's pretty great!",
         {-4, -12, -8, -8}},
        {"Some readers prefer to have the text weighted towards the "
         "bottom of the display to limit the amount the eyes have to move to read a page. The "
         "Bottom template uses the medium sized text for the least amount of eye movement.",
         {-3, -13, 80, 0}}};

    void setText();
    void moveSelection(int delta);
};
