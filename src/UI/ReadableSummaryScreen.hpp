#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class ReadableSummaryScreen : public Screen, public EinkView {
public:
    ReadableSummaryScreen(PreferencesStore &prefs, FileSystem &fileSystem,
                          std::shared_ptr<Readable> readable, DisplaySystem &display,
                          Renderer &renderer)
        : EinkView(display), prefs_(prefs), fileSystem_(fileSystem), readable_(std::move(readable)),
          renderer_(renderer), display_(display), dismissLabelView_(display, "Dismiss", ArrowUp),
          showUnreadLabelView_(display, "Show Unread", ArrowDown, ArrowSide::Right),
          backgroundView_(display, 5) {}

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;
    auto checkAtEnd() -> bool;
    void render(DisplaySystem &display) override;
    void restoreRendererStream();

    void processCommand(RemoteCommand command) override;

    void setPlacemark();
    void setPlaceInReadable(std::shared_ptr<EPubPlace> placeInReadable);

protected:
    PreferencesStore &prefs_;
    FileSystem &fileSystem_;
    std::shared_ptr<Readable> readable_;
    Renderer &renderer_;
    DisplaySystem &display_;

    bool showUnread_ = false;
    std::shared_ptr<EPubPlace> placeInReadable_;
    std::shared_ptr<TxtPlace> newPlace_;
    std::shared_ptr<TxtPlace> prevPlace_;
    std::shared_ptr<TxtPlace> preservePlace_;
    std::shared_ptr<RendererStream> rendererStream_;
    bool atEnd_ = false;

    ArrowLabelView dismissLabelView_;
    ArrowLabelView showUnreadLabelView_;

    BoxView backgroundView_;
};
