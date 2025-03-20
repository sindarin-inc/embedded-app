#pragma once

#include <utility>

#include "Displays/TextInsets.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/KindleBook.hpp"
#include "Models/KindlePageResponse.hpp"
#include "Models/Place.hpp"
#include "Renderers/Renderer.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/NewReadablePopupMenuScreen.hpp"
#include "UI/Screen.hpp"

class KindleReadableScreen : public EinkView, public Screen {
public:
    enum class State {
        IDLE,
        CONNECTING,
        LOADING_CONTENT,
        DISPLAY_CONTENT,
        LOADING_BACKGROUND,
        ERROR
    };

    KindleReadableScreen(const std::shared_ptr<RendererStream> &stream, DisplaySystem &display,
                         const std::shared_ptr<KindleBook> &kindleBook, PreferencesStore &prefs,
                         Telemetry &telemetry, FileSystem &fileSystem,
                         LightingController &lightingController, LocalStore &localStore,
                         Font *font);

    ~KindleReadableScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;
    void setState(State newState);
    void dismissScreen();

protected:
    std::shared_ptr<RendererStream> stream_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    FileSystem &fileSystem_;
    LightingController &lightingController_;
    LocalStore &localStore_;

    void setLoadingMessage(const std::string &message);
    auto connectToNetwork(bool useBleOnly, std::string &errorMessage) -> bool;
    void loadKindleBookPage();
    void disconnectNetworkConnections();

    // Handle page navigation
    // Load the next kindle page from API
    void loadNextKindlePage();
    // Load the previous kindle page from API
    void loadPreviousKindlePage();
    // Navigate locally within the current renderer buffer
    void navigateNext();
    void navigatePrevious();
    // Clear screen and prepare for rendering content
    void clearScreenForContent();
    // Turn to the next page in the renderer buffer
    void turnPage();
    // Check if we're at the end of the currently loaded content
    auto checkNearEndOfBuffer() -> bool;

private:
    std::shared_ptr<KindleBook> kindleBook_;
    State state_{State::CONNECTING};
    bool firstKindleLoad_{true};
    bool dismissed_{false};
    std::shared_ptr<CancelFlag> cancelFlag_;
    bool nextPageLoading_{false}; // Flag to track if we're already loading the next page
    std::shared_ptr<NewReadablePopupMenuScreen> popupMenuScreen_;
    std::shared_ptr<Screen> currentScreen_;

    // Current page information
    int currentPage_{0};
    int totalPages_{0};
    std::string currentPageText_;
    std::string currentPageImageUrl_;

    // Local paging information
    std::shared_ptr<Place> currentPlace_ = nullptr; // Current place in the renderer
    std::shared_ptr<Place> newPlace_ = nullptr;     // Next place to render
    bool atEnd_ = false;
    const int bufferThreshold_ = 1000; // Characters threshold to preload more content
    uint32_t lastRenderedPageSize_ = 0; // Size of the last rendered page in bytes

    MultiLineTextView loadingMessageView_;
    MultiLineTextView errorMessageView_;
    
    void showPopupMenu();
    void setCurrentScreen(std::shared_ptr<Screen> screen) { currentScreen_ = screen; }
};
