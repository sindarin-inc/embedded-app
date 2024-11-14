#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Menu/MenuItemTOC.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class ReadableTableOfContentsScreen : public Screen, public EinkView {
public:
    ReadableTableOfContentsScreen(PreferencesStore &prefs, std::shared_ptr<Readable> readable,
                                  Renderer &renderer, DisplaySystem &display, Telemetry &telemetry)
        : EinkView(display), prefs_(prefs), readable_(std::move(readable)), renderer_(renderer),
          display_(display), telemetry_(telemetry), menuView_(display),
          backLabelView_(display, "Dismiss") {
        menuView_.padding = 1;
        menuView_.itemsPerPage = 11;
    }

    void addMenuItem(const std::shared_ptr<MenuItem> &item) { menuView_.addMenuItem(item); }

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    void loadMenuItems();

    void setPlacemark();
    void setNearestChapterSelection(const std::shared_ptr<Place> &placeInFile);

protected:
    PreferencesStore &prefs_;
    std::shared_ptr<Readable> readable_;
    Renderer &renderer_;
    DisplaySystem &display_;
    Telemetry &telemetry_;
    MenuView menuView_;

    ArrowLabelView backLabelView_;

    void moveSelection(int delta);
};
