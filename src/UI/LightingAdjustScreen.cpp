#include "LightingAdjustScreen.hpp"

#include "UI/Bitmaps/Lightbulb.hpp"

#define LIGHTING_STEP_INCREMENT 7

LightingAdjustScreen::LightingAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                                           LightingController &lightingController,
                                           Telemetry &telemetry)
    : Screen(), EinkView(display), prefs_(prefs), lightingController_(lightingController),
      telemetry_(telemetry),
      lightbulbView_(display, LIGHTBULB_BITS, Lightbulb_width, Lightbulb_height), barView_(display),
      fillView_(display), firstLine_(display, "Press up and down"),
      secondLine_(display, "to adjust brightness"), leftArrowView_(display, ArrowLeft),
      rightArrowView_(display, ArrowRight) {
    fillView_.fillColor = display_.fg;
}

auto LightingAdjustScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    int spacing = 8;

    // Sublayout for centering all the main content in the view
    context.setSublayoutFrame(
        {{0, 0}, {size.width, size.height}}, LayoutOptionsSizeToFit | LayoutOptionsAlignCenter,
        [this, spacing](LayoutContext &context, SolSize size) -> SolSize {
            // Sublayout for horizontally centering the lightbulb and bar
            auto bulbAndBarRect = context.setSublayoutFrame(
                {{0, 0}, {size.width, size.height}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal,
                [this, spacing](LayoutContext &context, SolSize size) -> SolSize {
                    int x = 0, y = 0;
                    auto lightbulbRect = context.setViewFrame(lightbulbView_, {{x, y}, {0, 0}},
                                                              LayoutOptionsSizeToFit);
                    x += lightbulbRect.size.width + spacing;

                    SolRect barRectFrame = {{x, y}, {10, lightbulbRect.size.height}};

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

    // Put the arrow on the right and left -- either is fine to exit the screen
    // TODO(256displayonly): Fix plz
    auto arrowWidth = 5;
    context.setViewSizeInRect(leftArrowView_, {arrowWidth, 0}, SolRect::fromSize(size),
                              LayoutOptionsSizeToFit | LayoutOptionsAlignLeft |
                                  LayoutOptionsAlignCenterVertical);

    context.setViewSizeInRect(rightArrowView_, {arrowWidth, 0}, SolRect::fromSize(size),
                              LayoutOptionsSizeToFit | LayoutOptionsAlignRight |
                                  LayoutOptionsAlignCenterVertical);

    return size;
}

void LightingAdjustScreen::drawInRect(SolRect rect) {
    lightbulbView_.draw();
    barView_.draw();
    fillView_.draw();
    firstLine_.draw();
    secondLine_.draw();
    leftArrowView_.draw();
    rightArrowView_.draw();
}

void LightingAdjustScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect());
    draw();

    display.updateDisplay(firstLoad_ ? DisplaySystem::UpdateType::FULL
                                     : DisplaySystem::UpdateType::FAST);
    firstLoad_ = false;
}

void LightingAdjustScreen::logTelemetry(int setting, std::string direction) {
    DispatchGetCore0BackgroundQueue()->dispatch([this, setting, direction]() {
        uint16_t channel1, channel0;
        lightingController_.readAmbientLightData(channel1, channel0);
        int darkMode = this->prefs_.darkMode.get().value_or(false) ? 1 : 0;
        // Grab the current offset
        auto offset = prefs_.brightnessOffset.get().value_or(0);
        telemetry_.logMetric(TelemetryField::METRIC_LIGHTING, setting,
                             {{"dir", direction},
                              {"ch1", channel1},
                              {"ch0", channel0},
                              {"dark", darkMode},
                              {"duty", lightingController_.getCurrentDutyCycle()},
                              {"pred", lightingController_.readPredictedDutyCycle()},
                              {"off", offset}});
    });
}

void LightingAdjustScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::LEFT:
    case RemoteCommand::RIGHT:
    case RemoteCommand::ENTER:
    case RemoteCommand::BACK:
        back();
        break;
    case RemoteCommand::UP: {
        auto setting =
            std::min(lightingController_.getCurrentPercent() + LIGHTING_STEP_INCREMENT, 100);
        lightingController_.setOverrideValuePercent(setting, true);
        logTelemetry(setting, "up");
        setNeedsRerender();
        break;
    }
    case RemoteCommand::DOWN: {
        auto setting =
            std::max(lightingController_.getCurrentPercent() - LIGHTING_STEP_INCREMENT, 0);
        lightingController_.setOverrideValuePercent(setting, true);
        logTelemetry(setting, "down");
        setNeedsRerender();
        break;
    }
    default:
        break;
    }
}
