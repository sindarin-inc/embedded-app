#pragma once

// Uncomment to enable simulated connection loss for debugging reconnection issues
// #define DEBUG_KINDLE_RECONNECTION

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <functional>
#include <utility>

#include "Misc/CancelFlag.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/KindleBook.hpp"
#include "Models/KindlePageFlipper.hpp"
#include "Models/KindlePageResponse.hpp"
#include "Models/Place.hpp"
#include "Renderers/Renderer.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/KindleLastReadDialogScreen.hpp"
#include "UI/NewReadablePopupMenuScreen.hpp"
#include "UI/Screen.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

/**
 * KindleReadableScreen
 *
 * Displays Kindle book content and handles user interaction.
 * Delegates page management to KindlePageFlipper for API calls.
 */

class KindleReadableScreen : public EinkView, public Screen {
public:
    enum class State {
        CONNECTING,
        LOADING_INITIAL_CONTENT,
        LOADING_CONTENT,
        ERROR,
        DISPLAY_CONTENT,
        SHOWING_DIALOG,
        DISPLAY_PARTIAL_CONTENT,
    };

    KindleReadableScreen(DisplaySystem &display, const std::shared_ptr<KindleBook> &kindleBook,
                         PreferencesStore &prefs, Telemetry &telemetry, FileSystem &fileSystem,
                         LightingController &lightingController, LocalStore &localStore,
                         Font *font);

    ~KindleReadableScreen() override = default;

    void begin();
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;
    void setState(State newState);
    void dismissScreen();

protected:
    Renderer renderer_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    FileSystem &fileSystem_;
    LightingController &lightingController_;
    LocalStore &localStore_;

    void setErrorMessage(const std::string &message);
    using ConnectionCallback = std::function<void(bool connected)>;
    auto connectToNetwork(std::string &errorMessage, const ConnectionCallback &callback = nullptr,
                          int bleTimeoutMs = 10000) -> bool;
    void disconnectNetworkConnections();

    void navigateNext();
    void navigatePrevious();

    // Schedule a delayed loading message after 10 seconds
    void scheduleDelayedLoadingMessage();
    // Cancel any pending delayed message
    void cancelDelayedLoadingMessage();

private:
    std::shared_ptr<KindleBook> kindleBook_;
    std::shared_ptr<CancelFlag> cancelFlag_;
    KindlePageFlipper pageFlipper_;
    State state_{State::CONNECTING};

    std::shared_ptr<NewReadablePopupMenuScreen> popupMenuScreen_;
    std::shared_ptr<Readable> popupMenuReadable_;
    std::shared_ptr<Screen> currentScreen_;
    DisplaySystem::UpdateType updateType_ = DisplaySystem::UpdateType::FULL;

    // Local paging information
    std::shared_ptr<Place> currentPlace_ = nullptr; // Current place in the renderer
    std::shared_ptr<Place> newPlace_ = nullptr;     // Next place to render

    // Connection tracking
    bool isUsingBleConnection_ = false;
    int reconnectionAttempts_ = 0;
    static const int MAX_RECONNECTION_ATTEMPTS = 2;
    bool isReconnecting_ = false; // Prevents recursive reconnection attempts

    // Timing tracking
    int64_t bookOpenStartTime_ = 0;
    int64_t navigationStartTime_ = 0;
    int64_t connectionStartTime_ = 0;

    // Timer handles for UI tasks
    TimerHandle_t delayedMessageTimer_ = nullptr;
    std::shared_ptr<CancelFlag> reconnectionAttemptCancelFlag_;

#ifdef DEBUG_KINDLE_RECONNECTION
    // Timer for forced disconnect testing
    TimerHandle_t testDisconnectTimer_ = nullptr;
    bool testDisconnectExecuted_ = false;
#endif

    MultiLineTextView loadingMessageView_;
    MultiLineTextView delayedMessageView_;
    MultiLineTextView errorMessageView_;

    auto stateToString(State state) -> const char *;
    void showPopupMenu();
    void showTableOfContents();
    void showLastReadDialog(const SpiramString &dialogText);
    void handleLastReadDialogResponse(bool gotoLastReadPage);
    void submitLastReadDialogChoice(bool gotoLastReadPage, const std::string &loadingMessage);
    void setCurrentScreen(const std::shared_ptr<Screen> &screen);
    auto getCurrentScreen() -> std::shared_ptr<Screen> { return currentScreen_; }

    // Get telemetry metrics for the current state (matching Readable::telemetryPlacemarkMetrics)
    auto getTelemetryMetrics() -> std::unordered_map<std::string, TelemetryVariant> {
        std::unordered_map<std::string, TelemetryVariant> metrics;
        if (currentPlace_) {
            // Kindle books use TXT format, so we only need po (position offset)
            metrics["po"] = currentPlace_->getOffset();
        } else {
            metrics["po"] = 0U;
        }
        return metrics;
    }

    // Attempt to reconnect once after connection loss
    void attemptReconnection();
    // Cancel any pending reconnection attempt
    void cancelReconnectionAttempt();
    // Re-register page flipper callbacks with new cancel flag
    void updatePageFlipperCallbacks();
    // Timer callback for delayed message
    static void delayedMessageTimerCallback(TimerHandle_t timer);

#ifdef DEBUG_KINDLE_RECONNECTION
    // Methods for forced disconnect testing
    void scheduleTestDisconnect();
    void cancelTestDisconnect();
    static void testDisconnectTimerCallback(TimerHandle_t timer);
#endif

#ifdef TESTS
public:
    void setOnTestLastReadDialogShown(std::function<void()> callback) {
        log_i("TEST: registering last-read dialog shown callback on instance=%p",
              static_cast<void *>(this));
        onTestLastReadDialogShown_ = std::move(callback);
    }
    void setOnTestLastReadDialogChoice(std::function<void(bool)> callback) {
        log_i("TEST: registering last-read dialog choice callback on instance=%p",
              static_cast<void *>(this));
        onTestLastReadDialogChoice_ = std::move(callback);
    }

    void setOnTestLastReadDialogPostComplete(std::function<void(bool)> callback) {
        onTestLastReadDialogPostComplete_ = std::move(callback);
    }

private:
    std::function<void()> onTestLastReadDialogShown_;
    std::function<void(bool)> onTestLastReadDialogChoice_;
    std::function<void(bool)> onTestLastReadDialogPostComplete_;
#endif
};
