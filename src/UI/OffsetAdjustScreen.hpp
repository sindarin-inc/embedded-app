#ifndef OFFSET_ADJUST_SCREEN_HPP
#define OFFSET_ADJUST_SCREEN_HPP

#include <Renderer/Renderer.hpp>

#include "Misc/Battery.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Screen.hpp"

class OffsetAdjustScreen : public Screen, public EinkView {
public:
    OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry);

    OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry,
                       Renderer &renderer, std::shared_ptr<Place> place);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

protected:
    DisplaySystem &display_;
    Renderer renderer_;
    std::shared_ptr<Place> place_{nullptr};
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    Renderer &rendererRef_;

    BoxView outerBoxView_;
    BoxView backgroundBoxView_;

    MultiLineTextView explanationView_;

    auto setupInstructions() -> void;
};

#endif
