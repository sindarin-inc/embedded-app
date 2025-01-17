#pragma once

#include <memory>
#include <vector>

#include "UI/Bitmaps/Lightbulb.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Components/StackView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Screen.hpp"

// A box view that wants to be wayy bigger than it should be just to test constraining contnet size
class CrazyBoxView : public EinkView {
public:
    CrazyBoxView(DisplaySystem &display) : EinkView(display) {}
    ~CrazyBoxView() override = default;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override { return {300, 300}; }

    void drawInRect(SolRect rect) override {
        display_.drawRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                          display_.fg);
        auto insetRect = rect.inset(2);
        display_.drawRect(insetRect.origin.x, insetRect.origin.y, insetRect.size.width,
                          insetRect.size.height, display_.fg);
    }
};

// Example of a more complex view that combines other views in a layout
class LayoutTestScreen : public Screen, public EinkView {
public:
    LayoutTestScreen(DisplaySystem &display)
        : Screen(), EinkView(display),
          iconView_(display, LIGHTBULB_BITS, Lightbulb_width, Lightbulb_height),
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
          box1_(display), box2_(display), text1_(display, "Left"), crazyBoxView_(display) {}

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

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
    CrazyBoxView crazyBoxView_;
};
