#pragma once

#include <functional>
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
    RootUI(DisplaySystem &display, PreferencesStore &prefs, PowerManager &power,
           Telemetry &telemetry) noexcept
        : display_(display), prefs_(prefs), power_(power), telemetry_(telemetry) {}
    RootUI(RootUI const &) = delete;

    void setup(LocalStore &localStore, FileSystem &fileSystem,
               LightingController &lightingController);
    void handleBrowse(FileSystem &fileSystem,
                      const std::shared_ptr<GenericSettingsMenuScreen> &fsMenu,
                      const std::string &path = "/");

    void clearNeedsRerender() override;

    auto currentScreen() -> std::shared_ptr<Screen> { return screenStack_.top(); }
    void pushScreen(const std::shared_ptr<Screen> &screen, bool deferRender = false);
    void popScreen(bool clearDisplay = true);

    void popToMainMenu();

    auto needsRerender() -> bool override;
    void render(DisplaySystem &display) override;
    void renderIfNeeded() {
        if (needsRerender()) {
            display_.beginPowerOn();
            render(display_);
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

    void loop(RemoteCommand command) {
        loopRunning_ = true;
        if (command != RemoteCommand::NONE) {
            processCommand(command);
        }
        renderIfNeeded();
        loopRunning_ = false;
    }

    static auto getInstance() -> RootUI &;

#define TEMP_MSG_DURATION_DEFAULT 2000
#define TEMP_MSG_DURATION_NO_CLOSE 0

    // second parameter is a function that should be called in cleanup
    // By default, the duration is 2 seconds. If you set the duration to 0, it will not
    // automatically close and requires input to close.
    void showTemporaryMessage(
        const std::string &msg, const std::function<void()> &cleanupFunc = []() {},
        int duration = TEMP_MSG_DURATION_DEFAULT);

    // Helper for when you want default cleanup but also setting duration.
    void showTemporaryMessage(const std::string &msg, int duration) {
        showTemporaryMessage(
            msg, []() {}, duration);
    }

protected:
    ScreenStack screenStack_;
    bool loopRunning_ = false;
    const char *itemErrorMsg_ =
        "INTERNAL ERROR!\nThe item you tried to access\ncannot be read or "
        "found.\nPlease contact support.\n\nPress\u00a0\u00a0\ue000\u00a0\u00a0to return";
    const char *settingsRestartMsg_ = "Settings change requires restart...";

    DisplaySystem &display_;
    PreferencesStore &prefs_;
    PowerManager &power_;
    Telemetry &telemetry_;
};
