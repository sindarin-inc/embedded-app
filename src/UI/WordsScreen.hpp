#pragma once

#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Renderers/RendererStream.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/FirstHookTestScreen.hpp"
#include "UI/MsgPopup.hpp"
#include "UI/Screen.hpp"
#include "Version.hpp"
#include "config.h"

class WordsScreen : public ContainerScreen, public EinkView {
public:
    // This class is a blank canvas and it takes in bluetooth
    // characaters one at a time from the bluetooth server found in BLERemoteInput.hpp
    // and then buffers the characters until it gets a space bar or a new line character.
    // At that point, it writes that word the the display, advances the cursor, checks
    // if there's a newline needed (either through explcitly getting the new line or if the text
    // needs to wrap). Once the word is written to the display, it does a PARTIAL refresh of the
    // display and clears the buffer, waiting for the next word.

    WordsScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry,
                FileSystem &fileSystem)
        : EinkView(display), renderer_(display, GetTextRectFromPrefs(display, prefs)),
          prefs_(prefs), telemetry_(telemetry), fileSystem_(fileSystem), display_(display) {
        currentScreen_ = this;
        buffer_ = "";
        log_d("Set up a new words screen!");
    }

    ~WordsScreen() override = default;
    /*
        void draw() override;
        void update() override;
        void updateDisplay() override;
        void updateDisplay(DisplaySystem::UpdateType updateType, bool clearFrameBuffer, int
       vcomNudge, DisplaySystem::UpdateType altUpdateType, ScreenSide screen, Rect altRect)
       override; void processCommand(RemoteCommand command) override; auto layout(LayoutContext
       &context, Size size) -> Size override; void drawInRect(Rect rect) override;
      */

    void render(DisplaySystem &display) override {
        draw();
        display.updateDisplay(DisplaySystem::UpdateType::PARTIAL);
    }

    void processCommand(RemoteCommand command) override { log_d("process command! %d", command); }

    bool processCharacterInput(char c) {
        log_d("process character input! %c", c);
        buffer_ += c;
        if (c == ' ' || c == '\n' || c == '.' || c == ',' || c == '!' || c == '?' || c == ':' ||
            c == ';' || c == '-') {
            log_d("got a space or newline!");
            setNeedsRerender();
            return true;
        }
        return false;
    }

    void drawInRect(Rect rect) override {
        log_d("draw in rect!");
        auto streamFirstTime = std::make_shared<StdStringStream>(buffer_);
        renderer_.setStream(streamFirstTime);
        renderer_.renderFirstPage();
        // leftArrowView_.draw();
        // upperLeftTextView_.draw();
        // for (auto &view : menuItemViews_) {
        //     (*view).draw();
        // }
        // if (menuPageIndicatorViews_.size() > 1) {
        //     for (auto &view : menuPageIndicatorViews_) {
        //         (*view).draw();
        //     }
        // }
    }

    auto layout(LayoutContext &context, Size size) -> Size override {
        log_d("laying out words!");
        return size;
        // int x = 0;
        // int y = 0;
        // for (auto &view : menuItemViews_) {
        //     auto viewFrame = context.setViewFrame(
        //         *view, {{x, y}, size}, LayoutOptionsSizeToFitVertical | LayoutOptionsAlignTop);
        //     y += viewFrame.size.height + 1;
        // }
        // return {size.width, y};
    }

protected:
    Renderer renderer_;
    std::string buffer_;

    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    FileSystem &fileSystem_;

    DisplaySystem &display_;
};