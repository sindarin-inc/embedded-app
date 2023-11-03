#include <utility>

#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Renderers/RendererStream.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/MsgPopup.hpp"
#include "UI/ReadablePopupMenuScreen.hpp"
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
class ReadableScreen : public ContainerScreen, public EinkView {
public:
    ReadableScreen(const std::shared_ptr<RendererStream> &stream, DisplaySystem &display,
                   const std::shared_ptr<Readable> &readable, PreferencesStore &prefs,
                   Telemetry &telemetry, LocalStore &localStore, FileSystem &fileSystem,
                   LightingController &lightingController)
        : EinkView(display), stream_(stream), renderer_(display), readable_(readable),
          prefs_(prefs), telemetry_(telemetry), localStore_(localStore), fileSystem_(fileSystem),
          display_(display), popupMenuScreen_(display, prefs, readable, lightingController,
                                              renderer_, localStore_, telemetry_),
          leftArrowView_(display, ArrowLeft), rightArrowView_(display, ArrowRight),
          upArrowView_(display, ArrowUp), downArrowView_(display, ArrowDown) {

        currentScreen_ = this;
        renderer_.setStream(stream);

        std::string thePlace = prefs_.readablePlace.get(readable_->shortenedUuid.c_str());
        log_d("readablePlace: \"%s\"", thePlace.c_str());

        if (!prefs_.cesMode.get().value_or(false)) {
            readable_->place = Place::factory(thePlace, stream->getDocType());
        }

        // for the animation bits since we may end up coming back to this constructor after being
        // interrupted.
        newPlace_ = readable_->place;
        atEnd_ = this->checkAtEnd();
        pageSpeed_ = prefs_.autoPilotSpeed.get().value_or(0);

        log_d("Found place for %s = %s %s", readable_->shortenedUuid.c_str(),
              readable_->place->str().c_str(), atEnd_ ? "(at end)" : "");

        popupMenuScreen_.setBackCallback([this]() {
            autoPilotForceFull_ = true;
            this->setCurrentScreen(*this);
        });

        popupMenuScreen_.onMainMenu = [this]() {
            hidePopupMenu();
            prefs_.lastReadableId.remove();
            if (!prefs_.cesMode.get().value_or(false)) {
                if (pageSpeed_ != 0) {
                    xTimerStop(DisplaySystem::animationTimer, 0);
                }
            }
            setNeedsRerender();
            back();
        };

        popupMenuScreen_.onBookmarkToggle = [this]() {
            inBookmark_ = !inBookmark_;
            setNeedsRerender();
            writeOutBookmarkUpdate();
        };

        popupMenuScreen_.onReport = [this, &display]() {
            setNeedsRerender();
            auto metrics = readable_->telemetryPlacemarkMetrics();
            metrics["fw"] = kVersionString;
            telemetry_.logMetric(TelemetryField::METRIC_REPORT, readable_->uuid.c_str(), metrics);
            static std::vector<std::string> reportMsg = {
                "Thank you, our team will", "review this page for", "formatting issues."};
            auto *reportPopup = new MsgPopup(display, reportMsg);
            display.clearDisplay();
            this->setCurrentScreen(*reportPopup);
            reportPopup->setBackCallback([this, reportPopup, &display]() {
                display.clearDisplay();
                this->setCurrentScreen(*this);
                delete reportPopup;
            });
        };

        popupMenuScreen_.onRerender = [this]() { renderPopup_ = true; };
    }

    ~ReadableScreen() override = default;

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

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
    LocalStore &localStore_;
    FileSystem &fileSystem_;

    bool inBookmark_ = false;
    bool showingFirstTimeView_ = false;

    std::shared_ptr<Place> newPlace_ = nullptr;
    bool atEnd_ = false;
    bool shownIcons_ = false;
    bool renderPopup_ = false;
    bool autoPilotForceFull_ = false;

    DisplaySystem &display_;

    ReadablePopupMenuScreen popupMenuScreen_;

    // Must reset this to 0 on each page turn.
    unsigned int speedLinesToSkip_ = 0;
    unsigned long nextInterval_ = 0;
    // How many ms we should take to read the page
    int pageSpeed_ = 10000;
    bool autoPilotPause_ = false;

    ArrowView leftArrowView_;
    ArrowView rightArrowView_;
    ArrowView upArrowView_;
    ArrowView downArrowView_;

private:
    void incrementReadingTime();
    void turnPage();
    uint32_t readingStartTime_;
    LastArrowPress lastArrowPress_ = REMOTE_NONE;

    int numUps_ = 0;
};
