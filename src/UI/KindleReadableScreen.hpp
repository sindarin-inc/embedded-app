#pragma once

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
                          int bleTimeoutMs = 30000) -> bool;
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
    std::shared_ptr<Screen> currentScreen_;
    DisplaySystem::UpdateType updateType_ = DisplaySystem::UpdateType::FULL;

    // Local paging information
    std::shared_ptr<Place> currentPlace_ = nullptr; // Current place in the renderer
    std::shared_ptr<Place> newPlace_ = nullptr;     // Next place to render

    // Connection tracking
    bool isUsingBleConnection_ = false;
    int reconnectionAttempts_ = 0;
    static const int MAX_RECONNECTION_ATTEMPTS = 2;

    // Timer handles for UI tasks
    std::shared_ptr<CancelFlag> delayedMessageCancelFlag_;
    std::shared_ptr<CancelFlag> reconnectionAttemptCancelFlag_;

    MultiLineTextView loadingMessageView_;
    MultiLineTextView delayedMessageView_;
    MultiLineTextView errorMessageView_;

    auto stateToString(State state) -> const char *;
    void showPopupMenu();
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
};
