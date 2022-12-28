#include <memory>
#include <vector>

#include "UI/Bitmaps/Lightbulb.hpp"
#include "UI/Components/BoxView.hpp"
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

    auto layout(LayoutContext &context, Size size) -> Size override { return {300, 300}; }

    void drawInRect(Rect rect) override {
        display_.drawRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
                          display_.fg);
        auto insetRect = rect.inset(2);
        display_.drawRect(insetRect.origin.x, insetRect.origin.y, insetRect.size.width,
                          insetRect.size.height, display_.fg);
    }
};

// Example of a more complex view that combines other views in a layout
class ExampleView : public EinkView {
public:
    ExampleView(DisplaySystem &display)
        : EinkView(display), iconView_(display, lightbulbBits, Lightbulb_width, Lightbulb_height),
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
    ~ExampleView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {
        // inset all around by 2px
        int x = 2;
        int y = 2;
        size.width = size.width - 4;

        // Draw the icon vertically aligned on the left of the screen
        auto iconRect = context.setViewFrame(iconView_, {{x, y}, size},
                                             LayoutOptionsSizeToFit | LayoutOptionsAlignLeft |
                                                 LayoutOptionsAlignCenterVertical);

        // Create a sublayout to draw a bunch of stuff centered vertically on the remaining right
        // area of the screen. This is kinda like making a figma group of a bunch of layers then
        // center-aligning the whole group
        x = iconRect.origin.x + iconRect.size.width + 2;
        auto rightColumnRect = context.setSublayoutFrame(
            {{x, 0}, {size.width - x, size.height}},
            LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical,
            [this](LayoutContext &context, Size size) -> Size {
                int x = 0;
                int y = 0;

                // Now on the same top pixel as the logo, draw the top text
                int textWidth = size.width - x;
                auto topTextRect = context.setViewFrame(
                    topView_, {{x, y}, {textWidth, size.height}},
                    LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignTop);

                // Draw the next line immediately below but with a 2px offset
                y += topTextRect.size.height;
                auto bottomTextRect = context.setViewFrame(
                    bottomView_, {{x, y}, {textWidth, size.height}},
                    LayoutOptionsSizeToFit | LayoutOptionsAlignLeft | LayoutOptionsAlignTop);

                y += bottomTextRect.size.height;
                auto tripleBoxRect =
                    context.setViewFrame(tripleBoxView_, {{x, y}, {textWidth, 10}});

                y += tripleBoxRect.size.height + 2;

                // Create a sublayout with some horizontal elements
                auto sublayoutRect = context.setSublayoutFrame(
                    {{x, y}, {textWidth, 10}}, LayoutOptionsSizeToFit,
                    [this](LayoutContext &context, Size size) -> Size {
                        int x = 0;
                        int y = 0;

                        // Text with a box around it
                        auto text1Rect = context.setViewFrame(text1_, {{x + 4, y + 2}, {0, 0}},
                                                              LayoutOptionsSizeToFit);

                        auto box1Rect = context.setViewFrame(
                            box1_, {{x, y}, {text1Rect.size.width + 8, text1Rect.size.height + 4}});
                        y += box1Rect.size.height + 2;

                        auto box2Rect = context.setViewSizeInRect(box2_, {10, 10},
                                                                  {{x, y}, {size.width - x, 10}},
                                                                  LayoutOptionsAlignRight);
                        y += box2Rect.size.height;

                        return Size{size.width, y};
                    });

                y += sublayoutRect.size.height + 2;
                auto bottomBoxRect =
                    context.setViewFrame(bottomBoxView_, {{x, y}, {textWidth, 15}});

                y += bottomBoxRect.size.height + 2;
                auto evenSpacedWordsRect =
                    context.setViewFrame(evenSpacedWordsView_, {{x, y}, {textWidth, 15}});
                y += evenSpacedWordsRect.size.height + 2;

                auto crazyBoxRect =
                    context.setViewSizeInRect(crazyBoxView_, {size.width - 40, 15},
                                              {{x, y}, {size.width, 15}}, LayoutOptionsAlignRight);
                y += crazyBoxRect.size.height;

                return {size.width, y};
            });

        return {size.width, std::max(rightColumnRect.size.height, iconRect.size.height)};
    };

    void drawInRect(Rect rect) override {
        iconView_.draw();
        topView_.draw();
        bottomView_.draw();
        tripleBoxView_.draw();
        bottomBoxView_.draw();
        evenSpacedWordsView_.draw();
        box1_.draw();
        box2_.draw();
        text1_.draw();
        crazyBoxView_.draw();
    }

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

class LayoutTestScreen : public Screen {
public:
    LayoutTestScreen(DisplaySystem &display) : Screen(), view(display) {}

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool firstLoad = true;

    ExampleView view;
};
