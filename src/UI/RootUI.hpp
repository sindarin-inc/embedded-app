#pragma once

#include <vector>

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Battery.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Menu/GenericSettingsMenuScreen.hpp"
#include "UI/Screen.hpp"
#include "UI/ScreenStack.hpp"

class RootUI : public Screen {
public:
    RootUI(PreferencesStore &prefs, PowerManager &power, Telemetry &telemetry) noexcept
        : prefs_(prefs), power_(power), telemetry_(telemetry) {}
    RootUI(RootUI const &) = delete;

    void setup(DisplaySystem *display, LocalStore &localStore, FileSystem &fileSystem,
               LightingController &lightingController);
    void handleBrowse(FileSystem &fileSystem,
                      const std::shared_ptr<GenericSettingsMenuScreen> &fsMenu,
                      const std::string &path = "/");

    void clearNeedsRerender() override;

    auto currentScreen() -> std::shared_ptr<Screen> { return screenStack_.top(); }
    void pushScreen(const std::shared_ptr<Screen> &screen);
    void popScreen(bool clearDisplay = true);

    auto needsRerender() -> bool override;
    void render(DisplaySystem &display) override;
    void renderIfNeeded(DisplaySystem &display) {
        if (needsRerender()) {
            display.beginPowerOn();
            render(display);
            clearNeedsRerender();
        }
    };
    void processCommand(RemoteCommand command) override;
    auto getCurrentScreen() -> std::shared_ptr<Screen> {
        if (screenStack_.empty()) {
            return nullptr;
        }
        return screenStack_.top();
    }

    void loop(DisplaySystem &display, RemoteCommand command) {
        loopRunning_ = true;
        if (command != RemoteCommand::NONE) {
            processCommand(command);
        }
        renderIfNeeded(display);
        loopRunning_ = false;
    }

protected:
    ScreenStack screenStack_;
    bool loopRunning_ = false;
    const char *itemErrorMsg_ =
        "INTERNAL ERROR!\nThe item you tried to access\ncannot be read or "
        "found.\nPlease contact support.\n\nPress\u00a0\u00a0\ue000\u00a0\u00a0to return";
    const char *settingsRestartMsg_ = "Settings change requires restart...";

    PreferencesStore &prefs_;
    PowerManager &power_;
    DisplaySystem *display_ = nullptr;
    Telemetry &telemetry_;
};
