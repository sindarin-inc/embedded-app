#include "LightingAdjustScreen.hpp"

#include "UI/Bitmaps/Lightbulb.hpp"

#define LIGHTING_STEP_INCREMENT 10

LightingAdjustScreen::LightingAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                                           LightingController &lightingController)
    : Screen(), EinkView(display), prefs_(prefs), lightingController_(lightingController),
      lightbulbView_(display, lightbulbBits, Lightbulb_width, Lightbulb_height), barView_(display),
      fillView_(display), firstLine_(display, "Press up and down"),
      secondLine_(display, "to adjust brightness"), thirdLine_(display, "Let there be light!"),
      arrowView_(display) {
    fillView_.fillColor = display_.fg;
}

auto LightingAdjustScreen::layout(LayoutContext &context, Size size) -> Size {
    int y = 0;
    int spacing = 8;

    // Sublayout for centering all the main content in the view
    auto mainRect = context.setSublayoutFrame(
        {{0, 0}, {size.width, size.height}}, LayoutOptionsSizeToFit | LayoutOptionsAlignCenter,
        [this, spacing](LayoutContext &context, Size size) -> Size {
            // Sublayout for horizontally centering the lightbulb and bar
            auto bulbAndBarRect = context.setSublayoutFrame(
                {{0, 0}, {size.width, size.height}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal,
                [this, spacing](LayoutContext &context, Size size) -> Size {
                    int x = 0, y = 0;
                    auto lightbulbRect = context.setViewFrame(lightbulbView_, {{x, y}, {0, 0}},
                                                              LayoutOptionsSizeToFit);
                    x += lightbulbRect.size.width + spacing;

                    Rect barRectFrame = {{x, y}, {10, lightbulbRect.size.height}};

                    auto barRect = context.setViewFrame(barView_, barRectFrame);

                    auto lightingPercent = lightingController_.getCurrentPercent();
                    int fillHeight = (lightingPercent * barRect.size.height) / 100;
                    int fillTop = barRect.origin.y + barRect.size.height - fillHeight;

                    context.setViewFrame(fillView_, {{x, fillTop}, {10, fillHeight}});

                    y += lightbulbRect.size.height;
                    return {barRect.right(), y};
                });

            int y = 0;
            y += bulbAndBarRect.size.height + spacing;

            auto firstLineRect =
                context.setViewFrame(firstLine_, {{0, y}, {size.width, 0}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
            y += firstLineRect.size.height;

            auto secondLineRect =
                context.setViewFrame(secondLine_, {{0, y}, {size.width, 0}},
                                     LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
            y += secondLineRect.size.height;

            return {size.width, y};
        });
    y = mainRect.bottom() + 10;

    // Vertically center the main content above and then shove the easter egg below it
    // When the easter egg is hidden, the content will be centered vertically.
    context.setViewFrame(thirdLine_, {{0, y}, {size.width, 0}},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal);

    // Put the arrow on the right
    context.setViewFrame(arrowView_, {{0, 0}, {size.width, size.height}},
                         LayoutOptionsAlignRight | LayoutOptionsAlignCenterVertical |
                             LayoutOptionsSizeToFit);

    return size;
}

void LightingAdjustScreen::drawInRect(Rect rect) {
    lightbulbView_.draw();
    barView_.draw();
    fillView_.draw();
    firstLine_.draw();
    secondLine_.draw();
    // Easter eggs are cool.
    if (showThirdLine_) {
        thirdLine_.draw();
    }
    arrowView_.draw();
}

void LightingAdjustScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    // TODO: Unify needsRerender and setNeedsLayout?
    setNeedsLayout();
    setFrame({{0, 0}, {display.width(), display.height()}});
    draw();

    display.updateDisplay(firstLoad ? DisplaySystem::UpdateType::FULL
                                    : DisplaySystem::UpdateType::FAST);
    firstLoad = false;

    needsRerender = false;
}

void LightingAdjustScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        showThirdLine_ = true;
        firstLoad = true;
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        showThirdLine_ = false;
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    case RemoteCommand::PREV: {
        auto setting =
            std::min(lightingController_.getCurrentPercent() + LIGHTING_STEP_INCREMENT, 100);
        lightingController_.setOverrideValuePercent(setting);
        needsRerender = true;
        break;
    }
    case RemoteCommand::NEXT: {
        auto setting =
            std::max(lightingController_.getCurrentPercent() - LIGHTING_STEP_INCREMENT, 0);
        lightingController_.setOverrideValuePercent(setting);
        needsRerender = true;
        break;
    }
    case RemoteCommand::BACK:
        showThirdLine_ = false;
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    default:
        break;
    }
}
