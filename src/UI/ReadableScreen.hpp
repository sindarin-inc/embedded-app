#include <utility>

#include "Contents/Readable.hpp"
#include "Misc/Power.hpp"
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
    ReadableScreen(RendererStream *stream, DisplaySystem &display, Readable &readable,
                   PreferencesStore &prefs, Power &power, FileSystem &fileSystem)

        : renderer_(display), readable_(readable), prefs_(prefs),
          popup_(power, renderer_, prefs_, readable_), fileSystem_(fileSystem) {
        currentScreen_ = this;
        renderer_.setStream(stream);
        pageSpeed_ = prefs_.speed.get().value();
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

protected:
    Renderer renderer_;
    Readable &readable_;
    PreferencesStore &prefs_;

    PopupMenuScreen popup_;

    FileSystem &fileSystem_;

    bool inBookmark_ = false;

    unsigned long newPlace_ = 0;

    bool shownIcons_ = false;

    // Must reset this to 0 on each page turn.
    unsigned int speedLinesToSkip_ = 0;
    unsigned long timer_ = 0;
    unsigned long nextInterval_ = 0;
    // How many ms we should take to read the page
    unsigned int pageSpeed_ = 10000;

private:
    void incrementReadingTime();
    uint32_t readingStartTime_;
};
