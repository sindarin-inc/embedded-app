#include "Misc/Menu.hpp"
#include "UI/Components/PopupMenuView.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/LightingAdjustScreen.hpp"
#include "UI/ReadableSeekScreen.hpp"
#include "UI/ReadableTableOfContentsScreen.hpp"
#include "UI/Screen.hpp"

class ReadablePopupMenuScreen : public ContainerScreen, public View, public Menu {

public:
    ReadablePopupMenuScreen(DisplaySystem &display, PreferencesStore &prefs, Readable &readable,
                            LightingController &lightingController, Renderer &renderer)
        : display_(display), prefs_(prefs), readable_(readable), renderer_(renderer),
          popupMenuView_(display), lightingAdjustScreen_(display, prefs, lightingController),
          readableSeekScreen_(prefs, readable, renderer),
          readableTableOfContentsScreen_(prefs, readable, renderer, display) {
        this->setCurrentScreen(*this);

        lightingAdjustScreen_.setBackCallback([this]() {
            onRerender();
            setFirstLoad();
            this->setCurrentScreen(*this);
        });
        readableSeekScreen_.setBackCallback([this]() {
            setUpPopupMenu();
            onRerender();
            setFirstLoad();
            this->setCurrentScreen(*this);
        });

        readableTableOfContentsScreen_.setBackCallback([this]() {
            onRerender();
            setFirstLoad();
            this->setCurrentScreen(*this);
        });
    };
    ~ReadablePopupMenuScreen() override = default;

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void back() override;
    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void setUpPopupMenu();
    void preselectArchiveMenuItem();
    void preselectFirstItem();
    void preselectLastItem();

    std::function<void()> onMainMenu;
    std::function<void()> onBookmarkToggle;
    std::function<void()> onRerender;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    const Readable &readable_;
    Renderer &renderer_;
    PopupMenuView popupMenuView_;
    bool firstLoad_ = true;

    std::shared_ptr<MenuItemView> archiveMenuItemView_;
    std::shared_ptr<MenuItemView> unarchiveMenuItemView_;

    LightingAdjustScreen lightingAdjustScreen_;
    ReadableSeekScreen readableSeekScreen_;
    ReadableTableOfContentsScreen readableTableOfContentsScreen_;

    void addMenuItem(const MenuItem &newItem) override;
    void populatePopupMenu();
    void resetMenuItems() override {
        Menu::resetMenuItems();
        popupMenuView_.menuItemViews.clear();
    }
    void setFirstLoad() { firstLoad_ = true; }
};
