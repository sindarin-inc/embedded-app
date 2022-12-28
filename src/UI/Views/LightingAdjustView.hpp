#pragma once

#include "Sensors/LightingController.hpp"
#include "UI/Bitmaps/Lightbulb.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"

// TODO: I don't love having this in a separate file. I wonder how we can unify this with the screen
// and also make the compiler happy
class LightingAdjustView : public EinkView {
public:
    LightingAdjustView(DisplaySystem &display, LightingController &lightingController)
        : EinkView(display), lightingController_(lightingController),
          lightbulbView_(display, lightbulbBits, Lightbulb_width, Lightbulb_height),
          barView_(display), fillView_(display), firstLine_(display, "Press up and down"),
          secondLine_(display, "to adjust brightness"), thirdLine_(display, "Let there be light!"),
          arrowView_(display) {
        fillView_.fillColor = display_.fg;
    }
    ~LightingAdjustView() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override {
        int y = 0;
        int spacing = 8;

        // Sublayout for centering all the main content in the view
        auto mainRect = context.setSublayoutFrame(
            {0, 0, size.width, size.height}, LayoutOptionsSizeToFit | LayoutOptionsAlignCenter,
            [this, spacing](LayoutContext &context, Size size) -> Size {
                // Sublayout for horizontally centering the lightbulb and bar
                auto bulbAndBarRect = context.setSublayoutFrame(
                    {0, 0, size.width, size.height},
                    LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal,
                    [this, spacing](LayoutContext &context, Size size) -> Size {
                        int x = 0, y = 0;
                        auto lightbulbRect = context.setViewFrame(lightbulbView_, {x, y, 0, 0},
                                                                  LayoutOptionsSizeToFit);
                        x += lightbulbRect.size.width + spacing;

                        Rect barRectFrame = {x, y, 10, lightbulbRect.size.height};

                        auto barRect = context.setViewFrame(barView_, barRectFrame);

                        auto lightingPercent = lightingController_.getCurrentPercent();
                        int fillHeight = (lightingPercent * barRect.size.height) / 100;
                        int fillTop = barRect.origin.y + barRect.size.height - fillHeight;

                        auto fillView =
                            context.setViewFrame(fillView_, {x, fillTop, 10, fillHeight});

                        y += lightbulbRect.size.height;
                        return {barRect.right(), y};
                    });

                int x = 0, y = 0;
                y += bulbAndBarRect.size.height + spacing;

                auto firstLineRect =
                    context.setViewFrame(firstLine_, {0, y, size.width, 0},
                                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
                y += firstLineRect.size.height;

                auto secondLineRect =
                    context.setViewFrame(secondLine_, {0, y, size.width, 0},
                                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
                y += secondLineRect.size.height;

                return {size.width, y};
            });
        y = mainRect.bottom() + 10;

        // Vertically center the main content above and then shove the easter egg below it
        // When the easter egg is hidden, the content will be centered vertically.
        context.setViewFrame(thirdLine_, {0, y, size.width, 0},
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);

        // Put the arrow on the right
        context.setViewFrame(arrowView_, {0, 0, size.width, size.height},
                             LayoutOptionsAlignRight | LayoutOptionsAlignCenterVertical |
                                 LayoutOptionsSizeToFit);

        return size;
    }

    void drawInRect(Rect rect) override {
        lightbulbView_.draw();
        barView_.draw();
        fillView_.draw();
        firstLine_.draw();
        secondLine_.draw();
        // Easter eggs are cool.
        if (showThirdLine) {
            thirdLine_.draw();
        }
        arrowView_.draw();
    }

    bool showThirdLine;

protected:
    IconView lightbulbView_;
    BoxView barView_;
    BoxView fillView_;

    // TODO; These first two lines could be one view once we have MultiLineTextView
    SingleLineTextView firstLine_;
    SingleLineTextView secondLine_;
    SingleLineTextView thirdLine_;

    ArrowView arrowView_;

    LightingController &lightingController_;
};
