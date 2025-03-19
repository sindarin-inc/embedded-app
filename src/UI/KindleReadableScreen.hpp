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
#include "UI/Screen.hpp"

class KindleReadableScreen : public EinkView, public Screen {
public:
    enum class State { IDLE, CONNECTING, LOADING_CONTENT, DISPLAY_CONTENT, ERROR };

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
    void navigateNext();
    void navigatePrevious();

private:
    std::shared_ptr<KindleBook> kindleBook_;
    State state_{State::CONNECTING};
    bool firstKindleLoad_{true};
    bool dismissed_{false};
    std::shared_ptr<CancelFlag> cancelFlag_;

    // Current page information
    int currentPage_{0};
    int totalPages_{0};
    std::string currentPageText_;
    std::string currentPageImageUrl_;

    MultiLineTextView loadingMessageView_;
    MultiLineTextView errorMessageView_;
};
