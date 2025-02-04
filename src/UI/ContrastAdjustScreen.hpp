#pragma once

#include "Misc/Telemetry.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/IconView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Screen.hpp"

class ContrastAdjustScreen : public Screen, public EinkView {
public:
    ContrastAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                         LightingController &lightingController, Telemetry &telemetry);

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void logTelemetry(int setting, std::string direction);

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    LightingController &lightingController_;
    Telemetry &telemetry_;

    // Subviews
    IconView contrastIconView_;
    BoxView barView_;
    BoxView fillView_;
    // TODO; These first two lines could be one view once we have MultiLineTextView
    SingleLineTextView firstLine_;
    SingleLineTextView secondLine_;

    ArrowView leftArrowView_;
    ArrowView rightArrowView_;
};
