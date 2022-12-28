#include "LightingAdjustScreen.hpp"

#define LIGHTING_STEP_INCREMENT 10

LightingAdjustScreen::LightingAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                                           LightingController &lightingController)
    : Screen(), view_(display, lightingController), prefs_(prefs),
      lightingController_(lightingController) {}

void LightingAdjustScreen::render(DisplaySystem &display) {
    if (!needsRerender) {
        return;
    }

    // TODO: Unify needsRerender and setNeedsLayout?
    view_.setNeedsLayout();
    view_.setFrame({{0, 0}, {display.width(), display.height()}});
    view_.draw();

    display.updateDisplay(firstLoad ? DisplaySystem::UpdateType::FULL
                                    : DisplaySystem::UpdateType::FAST);
    firstLoad = false;

    needsRerender = false;
}

void LightingAdjustScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::CHANNEL_MINUS:
        view_.showThirdLine = true;
        firstLoad = true;
        needsRerender = true;
        break;
    case RemoteCommand::CHANNEL_PLUS:
        view_.showThirdLine = false;
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
        view_.showThirdLine = false;
        if (backCallback != nullptr) {
            backCallback();
        }
        break;
    default:
        break;
    }
}
