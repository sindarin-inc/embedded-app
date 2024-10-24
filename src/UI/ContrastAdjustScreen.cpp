#if CONFIG_DISPLAY_EINKET013TT1

#include "UI/ContrastAdjustScreen.hpp"

#include "Displays/DisplayEinkET013TT1.hpp"
#include "Misc/Math.hpp"
#include "UI/Bitmaps/ContrastIcon.xbm"

#define CONTRAST_STEP_INCREMENT 125
#define VCOM_MIN 3250
#define VCOM_MAX 4750

ContrastAdjustScreen::ContrastAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                                           LightingController &lightingController,
                                           Telemetry &telemetry)
    : Screen(), EinkView(display), display_(display), prefs_(prefs),
      lightingController_(lightingController), telemetry_(telemetry),
      contrastIconView_(display, CONTRAST_ICON_BITS, CONTRAST_ICON_WIDTH, CONTRAST_ICON_HEIGHT),
      barView_(display), fillView_(display), firstLine_(display, "Press up and down"),
      secondLine_(display, "to adjust contrast"), leftArrowView_(display, ArrowLeft),
      rightArrowView_(display, ArrowRight) {
    fillView_.fillColor = display_.fg;
}

auto ContrastAdjustScreen::layout(LayoutContext &context, Size size) -> Size {
    int spacing = 8;

    // Sublayout for centering all the main content in the view
    context.setSublayoutFrame(
        {{0, 0}, {size.width, size.height}}, LayoutOptionsSizeToFit | LayoutOptionsAlignCenter,
        [this, spacing](LayoutContext &context, Size size) -> Size {
            // Sublayout for horizontally centering the contrast icon and bar
            auto contrastAndBarRect = context.setSublayoutFrame(
                {{0, 0}, {size.width, size.height}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignCenterHorizontal,
                [this, spacing](LayoutContext &context, Size size) -> Size {
                    int x = 0, y = 0;
                    auto contrastIconRect = context.setViewFrame(
                        contrastIconView_, {{x, y}, {0, 0}}, LayoutOptionsSizeToFit);
                    x += contrastIconRect.size.width + spacing;

                    Rect barRectFrame = {{x, y}, {10, contrastIconRect.size.height}};

                    auto barRect = context.setViewFrame(barView_, barRectFrame);

                    auto *driver = (dynamic_cast<DisplayEinkET013TT1 *>(&display_))->getDriver();
                    auto vcom = -1 * (driver->getVcom() - driver->getVcomNudge());
                    float vcomPct = (vcom - VCOM_MIN) / static_cast<float>(VCOM_MAX - VCOM_MIN);
                    auto fillHeight = vcomPct * barRect.size.height;
                    int fillTop = barRect.origin.y + barRect.size.height - fillHeight;

                    context.setViewFrame(fillView_,
                                         {{x, fillTop}, {10, static_cast<int>(fillHeight)}});

                    y += contrastIconRect.size.height;
                    return {barRect.right(), y};
                });

            int y = 0;
            y += contrastAndBarRect.size.height + spacing;

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
    context.setViewSizeInRect(leftArrowView_, {arrowWidth, 0}, Rect::fromSize(size),
                              LayoutOptionsSizeToFit | LayoutOptionsAlignLeft |
                                  LayoutOptionsAlignCenterVertical);

    context.setViewSizeInRect(rightArrowView_, {arrowWidth, 0}, Rect::fromSize(size),
                              LayoutOptionsSizeToFit | LayoutOptionsAlignRight |
                                  LayoutOptionsAlignCenterVertical);

    return size;
}

void ContrastAdjustScreen::drawInRect(Rect rect) {
    contrastIconView_.draw();
    barView_.draw();
    fillView_.draw();
    firstLine_.draw();
    secondLine_.draw();
    leftArrowView_.draw();
    rightArrowView_.draw();
}

void ContrastAdjustScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect());
    draw();

    display.updateDisplay(DisplaySystem::UpdateType::FULL);
}

void ContrastAdjustScreen::logTelemetry(int setting, std::string direction) {
    DispatchGetCore0BackgroundQueue()->dispatch([this, setting, direction]() {
        uint16_t channel1, channel0;
        lightingController_.readAmbientLightData(channel1, channel0);
        int darkMode = this->prefs_.darkMode.get().value_or(false) ? 1 : 0;
        auto *driver = (dynamic_cast<DisplayEinkET013TT1 *>(&display_))->getDriver();

        telemetry_.logMetric(TelemetryField::METRIC_LIGHTING, setting,
                             {{"dir", direction},
                              {"ch1", channel1},
                              {"ch0", channel0},
                              {"dark", darkMode},
                              {"vcom", driver->getVcom()}});
    });
}

void ContrastAdjustScreen::processCommand(RemoteCommand command) {
    auto *driver = (dynamic_cast<DisplayEinkET013TT1 *>(&display_))->getDriver();

    switch (command) {
    case RemoteCommand::LEFT:
    case RemoteCommand::RIGHT:
    case RemoteCommand::ENTER:
    case RemoteCommand::BACK:
        back();
        break;
    case RemoteCommand::UP: {
        auto setting = -1 * std::min(-1 * (driver->getVcom() - driver->getVcomNudge()) +
                                         CONTRAST_STEP_INCREMENT,
                                     VCOM_MAX);
        driver->setVcom(setting);
        prefs_.vcom.set(setting);
        log_i("Vcom: %d, Nudge: %d: %d", driver->getVcom(), driver->getVcomNudge(),
              -1 * (driver->getVcom() - driver->getVcomNudge()));
        logTelemetry(setting, "up");
        setNeedsRerender();
        break;
    }
    case RemoteCommand::DOWN: {
        auto setting = -1 * std::max(-1 * (driver->getVcom() - driver->getVcomNudge()) -
                                         CONTRAST_STEP_INCREMENT,
                                     VCOM_MIN);
        driver->setVcom(setting);
        prefs_.vcom.set(setting);
        log_i("Vcom: %d, Nudge: %d: %d", driver->getVcom(), driver->getVcomNudge(),
              -1 * (driver->getVcom() - driver->getVcomNudge()));
        logTelemetry(setting, "down");
        setNeedsRerender();
        break;
    }
    default:
        break;
    }
}

#endif
