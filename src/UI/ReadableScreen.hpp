#include <utility>

#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Renderers/RendererStream.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/ContrastAdjustScreen.hpp"
#include "UI/FirstHookTestScreen.hpp"
#include "UI/LightingAdjustScreen.hpp"
#include "UI/Menu/NewMenuItemSingleLine.hpp"
#include "UI/MsgPopup.hpp"
#include "UI/NewReadablePopupMenuScreen.hpp"
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
                   LightingController &lightingController, LocalStore &localStore)
        : EinkView(display), stream_(stream),
          renderer_(display, GetTextRectFromPrefs(display, prefs), prefs), readable_(readable),
          prefs_(prefs), telemetry_(telemetry), fileSystem_(fileSystem),
          lightingController_(lightingController), display_(display),
          leftArrowView_(display, ArrowLeft), rightArrowView_(display, ArrowRight),
          upArrowView_(display, ArrowUp), downArrowView_(display, ArrowDown) {

        auto popupNavigationFactory = [this]() -> MenuItemSharedPtrVector {
            auto menuItems = std::make_shared<MenuItemVector>();

            auto menuItem = std::make_shared<NewMenuItemSingleLine>("Main Menu", [this]() {
                setCurrentScreen(nullptr);
                back();
            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Brightness", [this]() {
                auto brightness = std::make_shared<LightingAdjustScreen>(
                    display_, prefs_, lightingController_, telemetry_);

                brightness->setBackCallback([this]() {
                    this->setCurrentScreen(nullptr);
                    setNeedsRerender();
                });
                this->setCurrentScreen(brightness);
            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Contrast", [this]() {
                auto contrast = std::make_shared<ContrastAdjustScreen>(
                    display_, prefs_, lightingController_, telemetry_);

                contrast->setBackCallback([this]() {
                    this->setCurrentScreen(nullptr);
                    setNeedsRerender();
                });
                this->setCurrentScreen(contrast);
            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Report Page", []() {});

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Archive", []() {});

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Contents", [this]() {
                auto toc = std::make_shared<ReadableTableOfContentsScreen>(
                    prefs_, readable_, renderer_, display_, telemetry_);

                auto stream = renderer_.getStream();
                if (stream->isFor(DocType::EPUB) &&
                    std::dynamic_pointer_cast<EPubFile>(stream)->hasToc()) {
                    toc->loadMenuItems();
                    toc->setNearestChapterSelection(renderer_.getCurrentPlace());
                    toc->setFirstLoad();
                }

                toc->setBackCallback([this]() {
                    this->setCurrentScreen(nullptr);
                    setNeedsRerender();
                });
                this->setCurrentScreen(toc);
            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Back to Text", [this]() {
                this->setCurrentScreen(nullptr);
                setNeedsRerender();
            });

            menuItems->emplace_back(menuItem);

            return menuItems;
        };

        auto popupContextFactory = [this]() -> MenuItemSharedPtrVector {
            auto menuItems = std::make_shared<MenuItemVector>();

            auto menuItem = std::make_shared<NewMenuItemSingleLine>("Dictionary", []() {});

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Summary", [this]() {

            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("History", []() {});

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Bookmark", []() {});

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Footnotes", [this]() {

            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Ask a Book", [this]() {

            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Back to Text", [this]() {
                this->setCurrentScreen(nullptr);
                setNeedsRerender();
            });

            menuItems->emplace_back(menuItem);

            return menuItems;
        };

        auto popupStyleFactory = [this]() -> MenuItemSharedPtrVector {
            auto menuItems = std::make_shared<MenuItemVector>();

            auto menuItem = std::make_shared<NewMenuItemSingleLine>("Text Size", [this]() {

            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Move Page", []() {});

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Resize Page", [this]() {

            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Spacing", [this]() {

            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Dark Mode", [this]() {

            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Autopilot", [this]() {

            });

            menuItems->emplace_back(menuItem);

            menuItem = std::make_shared<NewMenuItemSingleLine>("Back to Text", [this]() {
                this->setCurrentScreen(nullptr);
                setNeedsRerender();
            });

            menuItems->emplace_back(menuItem);

            return menuItems;
        };

        popupMenuScreen_ = std::make_shared<NewReadablePopupMenuScreen>(
            display, prefs, fileSystem, readable, lightingController, renderer_, telemetry_,
            localStore, stream_);

        popupMenuScreen_->addTab("Popup", popupNavigationFactory);

        popupMenuScreen_->addTab("Context", popupContextFactory);

        popupMenuScreen_->addTab("Style", popupStyleFactory);

        popupMenuScreen_->populateMenuView();

        readableSummaryScreen_ = std::make_shared<ReadableSummaryScreen>(
            prefs, fileSystem, readable, display, renderer_);

        renderer_.setStream(stream);

        std::string thePlace = prefs_.readablePlace.get(readable_->shortenedUuid.c_str());
        if (prefs_.cesMode.get().value_or(false)) {
            // For CES mode we intentionally want to ignore the saved place
            thePlace = "";
        }

        log_d("readablePlace: \"%s\"", thePlace.c_str());
        readable_->place = Place::factory(thePlace, stream->getDocType());

        // for the animation bits since we may end up coming back to this constructor after being
        // interrupted.
        newPlace_ = readable_->place;
        atEnd_ = this->checkAtEnd();
        pageSpeed_ = prefs_.autoPilotSpeed.get().value_or(0);

        log_d("Found place for %s = %s %s", readable_->shortenedUuid.c_str(),
              readable_->place->str().c_str(), atEnd_ ? "(at end)" : "");

        popupMenuScreen_->setBackCallback([this]() {
            autoPilotForceFull_ = true;
            this->setCurrentScreen(nullptr);
        });

        popupMenuScreen_->onMainMenu = [this]() {
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

        popupMenuScreen_->onBookmarkToggle = [this]() {
            inBookmark_ = !inBookmark_;
            setNeedsRerender();
            writeOutBookmarkUpdate();
        };

        popupMenuScreen_->onReport = [this, &display]() {
            setNeedsRerender();
            auto metrics = readable_->telemetryPlacemarkMetrics();
            metrics["fw"] = SemanticVersionString();
            auto insets = GetTextInsetFromPrefs(display, prefs_);
            metrics["in_l"] = insets.left;
            metrics["in_r"] = insets.right;
            metrics["in_t"] = insets.top;
            metrics["in_b"] = insets.bottom;
            metrics["text_size"] = prefs_.font.get().value_or(PREFS_SOL_SMALL);
            metrics["line_spacing"] = prefs_.lineSpacing.get().value_or(0);
            telemetry_.logMetric(TelemetryField::METRIC_REPORT, readable_->uuid.c_str(), metrics);
            auto reportPopup = std::make_shared<MsgPopup>(
                display, "Thank you, our team will\nreview this page for\nformatting issues.");
            display.clearDisplay();
            this->setCurrentScreen(reportPopup);
            reportPopup->setBackCallback([this, &display]() {
                display.clearDisplay();
                this->setCurrentScreen(nullptr);
            });
        };

        popupMenuScreen_->onRerender = [this]() { renderPopup_ = true; };

        readableSummaryScreen_->setBackCallback([this, &display]() {
            renderedPartialSummary_ = true;
            display.clearDisplay();
            this->setCurrentScreen(nullptr);
        });
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

    // auto setSelfShared(std::shared_ptr<Screen> readableScreenThis) {
    //     thisShared_ = readableScreenThis;
    // }

protected:
    void setCurrentScreen(std::shared_ptr<Screen> screen);

    auto getCurrentScreen() -> std::shared_ptr<Screen> { return currentScreen_; }

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
    bool showingFirstTimeView_ = false;

    std::shared_ptr<Place> newPlace_ = nullptr;
    bool atEnd_ = false;
    bool shownIcons_ = false;
    bool renderPopup_ = false;
    bool renderedPartialSummary_ = false;
    bool autoPilotForceFull_ = false;

    DisplaySystem &display_;

    std::shared_ptr<NewReadablePopupMenuScreen> popupMenuScreen_;
    std::shared_ptr<ReadableSummaryScreen> readableSummaryScreen_;

    std::vector<std::shared_ptr<Screen>> hookScreens_;

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

    std::shared_ptr<Screen> currentScreen_{nullptr};
    // std::shared_ptr<Screen> thisShared_{nullptr};

private:
    void incrementReadingTime();
    void turnPage();
    uint32_t readingStartTime_{};
    LastArrowPress lastArrowPress_ = REMOTE_NONE;

    int numUps_ = 0;
};
