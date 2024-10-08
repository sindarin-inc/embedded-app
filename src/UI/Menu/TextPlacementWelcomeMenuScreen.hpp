#pragma once

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Renderers/Renderer.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/RendererView.hpp"
#include "UI/Menu/NewMenuView.hpp"
#include "UI/RootUI.hpp"
#include "UI/Screen.hpp"

class TextPlacementWelcomeMenuScreen : public Screen, public EinkView {
public:
    TextPlacementWelcomeMenuScreen(DisplaySystem &display);

    void addMenuItem(const std::shared_ptr<NewMenuItem> &item);
    void processCommand(RemoteCommand command) override;

    [[nodiscard]] auto getCurrentTextSelection() const -> uint8_t { return textSelection_; }

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;
    void render(DisplaySystem &display) override;

private:
    NewMenuView menuView_;
    Renderer renderer_;
    RendererView rendererView_;
    ArrowView leftArrowView_;
    ArrowView rightArrowView_;
    uint8_t textSelection_ = 0;

    std::string introText_ =
        "These screens will show you three template views for text placement. You can select your "
        "preferred text placement. Don't worry, you can always change it later and customize even "
        "further in the Reading Settings menu!\n\n\n";

    std::vector<std::tuple<std::string, Inset>> texts_ = {
        {"This is an example of the Comfortable template which is the default.\n\nIt features "
         "small text, average line spacing, and large margins.",
         {0, 0, 0, 0}},
        {"This is the Condensed template for those who like to pack as many words on the page as "
         "possible. The text has the smallest line spacing and takes up the most space on the "
         "screen.\n\nSee how much extra filler text we have to write here just to fill up the "
         "page? It's pretty great!",
         {-8, -8, -8, -8}},
        {"Some readers prefer to have the text weighted towards the "
         "bottom of the display to limit the amount the eyes have to move to read a page. The "
         "Bottom template uses the medium sized text for the least amount of eye movement.",
         {-8, -8, 80, 0}}};

    void setText();
    void moveSelection(int delta);
};
