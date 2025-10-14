#ifndef RESIZE_BOX_SCREEN_HPP
#define RESIZE_BOX_SCREEN_HPP

#include <Renderer/Renderer.hpp>

#include "Misc/Battery.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Screen.hpp"

// Eventually we'll want to refactor this with the OffsetAdjustScreen
// since so much of the functionality is the same
// This is just to test that this concept works ok.
class ResizeBoxScreen : public Screen, public EinkView {
public:
    ResizeBoxScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry);
    ResizeBoxScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry,
                    Renderer &renderer, std::shared_ptr<Place> place);

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

protected:
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

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
