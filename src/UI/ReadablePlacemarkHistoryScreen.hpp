#pragma once

#include <Renderer/Renderer.hpp>
#include <memory>
#include <utility>
#include <vector>

#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Models/UserReadablePlacemarkHistory.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Menu/MenuItemTOC.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class ReadablePlacemarkHistoryScreen : public Screen, public EinkView {
public:
    ReadablePlacemarkHistoryScreen(PreferencesStore &prefs, FileSystem &fileSystem,
                                   std::shared_ptr<Readable> readable, Renderer &renderer,
                                   DisplaySystem &display, Telemetry &telemetry)
        : EinkView(display), prefs_(prefs), readable_(std::move(readable)), renderer_(renderer),
          display_(display), telemetry_(telemetry), fileSystem_(fileSystem), menuView_(display),
          backLabelView_(display, "Dismiss") {
        menuView_.padding = 1;
        menuView_.itemsPerPage = 11;
    }

    void addMenuItem(const std::shared_ptr<MenuItem> &item) { menuView_.addMenuItem(item); }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    void loadMenuItems();
    void setPlacemark(const std::shared_ptr<Place> &place);

protected:
    PreferencesStore &prefs_;
    std::shared_ptr<Readable> readable_;
    Renderer &renderer_;
    DisplaySystem &display_;
    Telemetry &telemetry_;
    FileSystem &fileSystem_;
    MenuView menuView_;

    ArrowLabelView backLabelView_;

    void moveSelection(int delta);
};
