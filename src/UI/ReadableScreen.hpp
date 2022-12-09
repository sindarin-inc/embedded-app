#include <utility>

#include "Misc/Power.hpp"
#include "Models/Readable.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/PopupMenuScreen.hpp"
#include "UI/Screen.hpp"
#include "config.h"

enum BookmarkStatus { BOOKMARK_OFF = 0, BOOKMARK_ON = 1 };

struct Bookmark {
    uint32_t firstPlace;
    uint32_t lastPlace;
    BookmarkStatus status;
};

class ReadableScreen : public ContainerScreen {
public:
    ReadableScreen(RendererStream *stream, DisplaySystem &display, const Readable &readable,
                   PreferencesStore &prefs, Power &power, FileSystem &fileSystem)

        : renderer_(display), firstTimeRenderer_(display), readable_(readable), prefs_(prefs),
          popup_(power, renderer_, prefs_, readable_), fileSystem_(fileSystem), display_(display) {
        currentScreen_ = this;
        renderer_.setStream(stream);
        pageSpeed_ = prefs_.autoPilotSpeed.get().value();
        // Since we copy in the readable from RootUI and RootUI only creates
        // the readable at the initial MainMenu creation, the place of that
        // readable may be stale. Hence we must re-load it from the prefs.
        // Not the prettiest of fixes, but passing the readable by reference
        // seems to not work with some of the const requirements by the lamdas.
        readable_.place = prefs_.readablePlace.get(readable_.shortenedUuid.c_str());
        // for the animation bits since we may end up coming back to this constructor after being
        // interrupted.
        newPlace_ = readable_.place;

        log_d("Found place for %s = %d", readable_.shortenedUuid.c_str(), readable_.place);
        popup_.backCallback = [this]() { this->setCurrentScreen(*this); };
    }

    ~ReadableScreen() override = default;

    void setLeftPopupButton(const std::string &title, std::function<void(void)> callback) {
        popup_.setLeftPopupButton(title, std::move(callback));
    }

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void writeOutBookmarkUpdate();
    void updateBookmarkStatus();
    auto readOneBookmark(File &file, Bookmark &bookmark) -> bool;

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

    FileSystem &fileSystem_;

    bool inBookmark_ = false;
    bool ignoreNextPress_ = false;

    unsigned long newPlace_ = 0;

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
    uint32_t readingStartTime_;
};
