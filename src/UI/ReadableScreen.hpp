#include <utility>

#include "Misc/Battery.hpp"
#include "Models/Readable.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Renderers/RendererStream.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/LineMenuScreen.hpp"
#include "UI/PopupMenuScreen.hpp"
#include "UI/Screen.hpp"
#include "config.h"

class ReadableScreen : public ContainerScreen {
public:
    ReadableScreen(std::shared_ptr<RendererStream> stream, DisplaySystem &display,
                   const Readable &readable, PreferencesStore &prefs, Battery &battery,
                   FileSystem &fileSystem, LightingController &lightingController)

        : renderer_(display), firstTimeRenderer_(display), readable_(readable), prefs_(prefs),
          popup_(display, battery, renderer_, prefs_, readable_, lightingController),
          archiveMenu_(readable_, prefs_, display), fileSystem_(fileSystem), display_(display) {
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
        popup_.backCallback = [this]() { this->setCurrentScreen(*this); };
        archiveMenu_.backCallback = [this]() { this->setCurrentScreen(*this); };
    }

    ~ReadableScreen() override = default;

    void setLeftPopupButton(const std::string &title, std::function<void(void)> callback) {
        popup_.setLeftPopupButton(title, std::move(callback));
    }

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void writeOutBookmarkUpdate();
    void updateBookmarkStatus();

    void renderIcons(DisplaySystem &display);

protected:
    Renderer renderer_;
    Renderer firstTimeRenderer_;
    // Note that this is a copy of the readable from RootUI.
    // So when we change the place in this readable's state,
    // it doesn't persist past when this screen is closed.
    Readable readable_;
    PreferencesStore &prefs_;

    PopupMenuScreen popup_;

    LineMenuScreen archiveMenu_;

    FileSystem &fileSystem_;

    bool inBookmark_ = false;
    bool ignoreNextPress_ = false;

    unsigned long newPlace_ = 0;
    bool atEnd_ = false;

    bool shownIcons_ = false;

    DisplaySystem &display_;

    // Must reset this to 0 on each page turn.
    unsigned int speedLinesToSkip_ = 0;
    unsigned long nextInterval_ = 0;
    // How many ms we should take to read the page
    int pageSpeed_ = 10000;

    bool showAutopilotPlus_ = false;
    bool showAutopilotMinus_ = false;

private:
    void incrementReadingTime();
    void turnPage();
    auto checkAtEnd() -> bool;
    uint32_t readingStartTime_;
    int numUps_ = 0;
};
