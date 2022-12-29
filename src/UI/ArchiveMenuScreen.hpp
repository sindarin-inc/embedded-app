#pragma once

#include <memory>
#include <vector>

#include "Models/Readable.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Bitmaps/Lightbulb.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/StackView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/MenuScreen.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

// Example of a more complex view that combines other views in a layout
class ArchiveMenuScreen : public MenuScreen, public EinkView {
public:
    ArchiveMenuScreen(Power &power, Readable &readable, PreferencesStore &prefs,
                      DisplaySystem &display)
        : MenuScreen(power), EinkView(display),
          iconView_(display, lightbulbBits, Lightbulb_width, Lightbulb_height),
          topView_(display, "First row of text"), bottomView_(display, "Second row"),
          nestedTextView_(std::make_shared<SingleLineTextView>(display, "HI")),
          tripleBoxView_(display, {std::make_shared<BoxView>(display),
                                   std::make_shared<StackView>(
                                       display,
                                       (std::vector<std::shared_ptr<EinkView>>){
                                           std::make_shared<BoxView>(display), nestedTextView_})}),
          bottomBoxView_(display,
                         {std::make_shared<BoxView>(display), std::make_shared<BoxView>(display),
                          std::make_shared<SingleLineTextView>(display, "HIII")}),
          evenSpacedWordsView_(display, {std::make_shared<SingleLineTextView>(display, "its"),
                                         std::make_shared<SingleLineTextView>(display, "me,"),
                                         std::make_shared<SingleLineTextView>(display, "hi"),
                                         std::make_shared<SingleLineTextView>(display, "i'm"),
                                         std::make_shared<SingleLineTextView>(display, "the")}),
          box1_(display), box2_(display), text1_(display, "Left") {}

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool firstLoad = true;

protected:
    IconView iconView_;
    SingleLineTextView topView_;
    SingleLineTextView bottomView_;
    std::shared_ptr<SingleLineTextView> nestedTextView_;
    StackView tripleBoxView_;
    StackView bottomBoxView_;
    StackView evenSpacedWordsView_;

    BoxView box1_;
    BoxView box2_;
    SingleLineTextView text1_;
};
