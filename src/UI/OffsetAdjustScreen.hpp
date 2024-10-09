#ifndef OFFSET_ADJUST_SCREEN_HPP
#define OFFSET_ADJUST_SCREEN_HPP

#include "Misc/Battery.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class OffsetAdjustScreen : public Screen {
public:
    OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry);

    OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry,
                       Renderer &renderer, std::shared_ptr<Readable> readable);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    Renderer renderer_;
    std::shared_ptr<Readable> readable_{nullptr};
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    Renderer &rendererRef_;
};

#endif
