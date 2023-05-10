#include <utility>

#include "Misc/Battery.hpp"
#include "Models/Readable.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Renderers/RendererStream.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/ReadablePopupMenuScreen.hpp"
#include "UI/Screen.hpp"
#include "config.h"

enum LastArrowPress {
    REMOTE_NONE,
    REMOTE_UP,
    REMOTE_DOWN,
    REMOTE_LEFT,
    REMOTE_RIGHT,
    REMOTE_CENTER
};

class ReadableScreen : public ContainerScreen, public EinkView {
public:
    ReadableScreen(std::shared_ptr<RendererStream> stream, DisplaySystem &display,
                   const Readable &readable, PreferencesStore &prefs, Battery &battery,
                   FileSystem &fileSystem, LightingController &lightingController)
        : EinkView(display), renderer_(display), firstTimeRenderer_(display), readable_(readable),
          prefs_(prefs), fileSystem_(fileSystem), display_(display),
          popupMenuScreen_(display, prefs, readable_, lightingController, renderer_),
          leftArrowView_(display, ArrowLeft), rightArrowView_(display, ArrowRight),
          upArrowView_(display, ArrowUp), downArrowView_(display, ArrowDown) {

        currentScreen_ = this;
        renderer_.setStream(std::move(stream));

        pageSpeed_ = prefs_.autoPilotSpeed.get().value_or(0);
        // Since we copy in the readable from RootUI and RootUI only creates
        // the readable at the initial MainMenu creation, the place of that
        // readable may be stale. Hence we must re-load it from the prefs.
        // Not the prettiest of fixes, but passing the readable by reference
        // seems to not work with some of the const requirements by the lamdas.
        if (!prefs_.cesMode.get().value_or(false)) {
            readable_.place = prefs_.readablePlace.get(readable_.shortenedUuid.c_str());
        } else {
            readable_.place = 0;
        }
        // for the animation bits since we may end up coming back to this constructor after being
        // interrupted.
        newPlace_ = readable_.place;
        atEnd_ = this->checkAtEnd();

        log_d("Found place for %s = %" PRIu32 " %s", readable_.shortenedUuid.c_str(),
              readable_.place, atEnd_ ? "(at end)" : "");

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
            writeOutBookmarkUpdate();
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

protected:
    void processCommandDemoMode(RemoteCommand command);
    void processCommandAutoPilot(RemoteCommand command);
    void processCommandNormal(RemoteCommand command);
    void showPopupMenu();
    void hidePopupMenu();

    void drawArrows(bool eraseArrow);

    Renderer renderer_;
    Renderer firstTimeRenderer_;
    // Note that this is a copy of the readable from RootUI.
    // So when we change the place in this readable's state,
    // it doesn't persist past when this screen is closed.
    Readable readable_;
    PreferencesStore &prefs_;

    FileSystem &fileSystem_;

    bool inBookmark_ = false;
    bool ignoreNextPress_ = false;

    unsigned long newPlace_ = 0;
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
    auto checkAtEnd() -> bool;
    uint32_t readingStartTime_;
    LastArrowPress lastArrowPress_ = REMOTE_NONE;

    int numUps_ = 0;
};
