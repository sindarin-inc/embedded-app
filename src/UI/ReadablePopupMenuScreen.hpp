#include "Misc/Menu.hpp"
#include "Misc/Telemetry.hpp"
#include "Storage/LocalStore.hpp"
#include "UI/Components/PopupMenuView.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/LightingAdjustScreen.hpp"
#include "UI/ReadableDictionaryScreen.hpp"
#include "UI/ReadableSeekScreen.hpp"
#include "UI/ReadableSummaryScreen.hpp"
#include "UI/ReadableTableOfContentsScreen.hpp"
#include "UI/Screen.hpp"

class ReadablePopupMenuScreen : public ContainerScreen, public View, public Menu {

public:
    ReadablePopupMenuScreen(DisplaySystem &display, PreferencesStore &prefs,
                            const std::shared_ptr<Readable> &readable,
                            LightingController &lightingController, Renderer &renderer,
                            LocalStore &localStore, Telemetry &telemetry)
        : display_(display), prefs_(prefs), readable_(readable), renderer_(renderer),
          popupMenuView_(display), localStore_(localStore), telemetry_(telemetry),
          lightingAdjustScreen_(display, prefs, lightingController, telemetry),
          readableSeekScreen_(prefs, readable, renderer),
          readableTableOfContentsScreen_(prefs, readable, renderer, display, telemetry),
          readableSummaryScreen_(prefs, readable, display),
          readableDictionaryScreen_(prefs, readable, display) {
        this->setCurrentScreen(*this);

        lightingAdjustScreen_.setBackCallback([this]() {
            this->setCurrentScreen(*this);
            back();
        });

        readableSeekScreen_.setBackCallback([this]() {
            this->setCurrentScreen(*this);
            back();
        });

        readableTableOfContentsScreen_.setBackCallback([this]() {
            this->setCurrentScreen(*this);
            back();
        });

        readableSummaryScreen_.setBackCallback([this]() {
            this->setCurrentScreen(*this);
            back();
        });

        readableDictionaryScreen_.setBackCallback([this]() {
            this->setCurrentScreen(*this);
            back();
        });
    };
    ~ReadablePopupMenuScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void setUpPopupMenu();
    void preselectArchiveMenuItem();
    void preselectFirstItem();
    void preselectLastItem();

    void showTemporaryMessage(DisplaySystem &display, const std::string &message,
                              int duration = 2000);

    std::function<void()> onMainMenu;
    std::function<void()> onBookmarkToggle;
    std::function<void()> onReport;
    std::function<void()> onRerender;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    std::shared_ptr<Readable> readable_;
    Renderer &renderer_;
    PopupMenuView popupMenuView_;
    LocalStore &localStore_;
    Telemetry &telemetry_;

    bool firstLoad_ = true;
    int currentChapterIdx_ = -1;

    std::shared_ptr<MenuItemView> archiveMenuItemView_;
    std::shared_ptr<MenuItemView> unarchiveMenuItemView_;

    LightingAdjustScreen lightingAdjustScreen_;
    ReadableSeekScreen readableSeekScreen_;
    ReadableTableOfContentsScreen readableTableOfContentsScreen_;
    ReadableSummaryScreen readableSummaryScreen_;
    ReadableDictionaryScreen readableDictionaryScreen_;

    void addMenuItem(const MenuItem &newItem) override;
    void populatePopupMenu();
    void resetMenuItems() override {
        Menu::resetMenuItems();
        popupMenuView_.menuItemViews.clear();
    }
    void setFirstLoad() { firstLoad_ = true; }
};
