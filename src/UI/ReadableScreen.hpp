#include <utility>

#include "Misc/PowerManager.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Renderers/RendererStream.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/BookQuestionsScreen.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/ContrastAdjustScreen.hpp"
#include "UI/FirstHookTestScreen.hpp"
#include "UI/LightingAdjustScreen.hpp"
#include "UI/LineMenuScreen.hpp"
#include "UI/LineSpacingScreen.hpp"
#include "UI/Menu/MenuItemCheckable.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/Menu/ReadingSettingsMenuScreen.hpp"
#include "UI/NewReadablePopupMenuScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/ReadablePlacemarkHistoryScreen.hpp"
#include "UI/ReadableSummaryScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/Screen.hpp"
#include "Version.hpp"
#include "config.h"

enum LastArrowPress {
    REMOTE_NONE,
    REMOTE_UP,
    REMOTE_DOWN,
    REMOTE_LEFT,
    REMOTE_RIGHT,
    REMOTE_CENTER
};

// TODO: make the readable copy here in Spiram
class ReadableScreen : public Screen, public EinkView {
public:
    ReadableScreen(const std::shared_ptr<RendererStream> &stream, DisplaySystem &display,
                   const std::shared_ptr<Readable> &readable, PreferencesStore &prefs,
                   Telemetry &telemetry, FileSystem &fileSystem,
                   LightingController &lightingController, LocalStore &localStore, Font *font);

    ~ReadableScreen() override = default;

    // TODO: Someday we should update this layout method to use the new RendererView
    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void writeOutBookmarkUpdate();
    void updateBookmarkStatus();
    auto checkAtEnd() -> bool;
    auto getScreenCurrentCharOffset() -> uint32_t;

    // Used to retrieve the current spine idx and offset in the corresponding file.
    // See test/CatchTests/TestEPub.cpp
    auto getStreamPlace() -> renderer_defs::StreamPlace {
        return renderer_.getStreamPlace(readable_->place);
    }

    auto getStream() { return stream_; }

protected:
    void setCurrentScreen(const std::shared_ptr<Screen> &screen);

    auto getCurrentScreen() -> std::shared_ptr<Screen> { return currentScreen_; }

    void backUpAutopilot(int numLines = 1);
    void processCommandDemoMode(RemoteCommand command);
    void processCommandAutoPilot(RemoteCommand command);
    void processCommandNormal(RemoteCommand command);
    void showPopupMenu();
    void hidePopupMenu();

    void drawArrows(bool eraseArrow);

    std::shared_ptr<RendererStream> stream_;
    Renderer renderer_;
    std::shared_ptr<Readable> readable_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    FileSystem &fileSystem_;
    LightingController &lightingController_;

    bool firstLoad_ = true;
    bool inBookmark_ = false;

    std::shared_ptr<Place> newPlace_ = nullptr;
    bool atEnd_ = false;
    bool shownIcons_ = false;
    bool renderPopup_ = false;
    bool renderedPartialSummary_ = false;
    bool autoPilotForceFull_ = false;

    DisplaySystem &display_;

    LocalStore &localStore_;

    std::shared_ptr<NewReadablePopupMenuScreen> popupMenuScreen_;
    std::shared_ptr<ReadableSummaryScreen> readableSummaryScreen_{nullptr};
    std::shared_ptr<ReadablePlacemarkHistoryScreen> readablePlacemarkHistoryScreen_{nullptr};
    std::shared_ptr<BookQuestionsScreen> bookQuestionsScreen_{nullptr};

    std::vector<std::shared_ptr<Screen>> hookScreens_;

    // Must reset this to 0 on each page turn.
    unsigned int speedLinesToSkip_ = 0;
    unsigned long nextInterval_ = 0;
    // How many ms we should take to read the page
    int pageSpeed_ = 10000;
    bool autoPilotPause_ = false;

    std::shared_ptr<Screen> currentScreen_{nullptr};

private:
    void turnPage();
    uint32_t readingStartTime_{};
    LastArrowPress lastArrowPress_ = REMOTE_NONE;

    const char *notExistMessage_ =
        "This content doesn't have that feature.\nPress any button to continue.";

    int numUps_ = 0;
};
