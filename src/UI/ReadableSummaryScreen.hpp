#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class ReadableSummaryScreen : public Screen, public EinkView {
public:
    ReadableSummaryScreen(PreferencesStore &prefs, FileSystem &fileSystem,
                          std::shared_ptr<Readable> readable, DisplaySystem &display,
                          Renderer &renderer)
        : EinkView(display), prefs_(prefs), fileSystem_(fileSystem), readable_(std::move(readable)),
          renderer_(renderer), display_(display), upperLeftTextView_(display, upperLeftText_),
          bottomRightTextView_(display, bottomRightText_), leftArrowView_(display, ArrowUp),
          rightArrowView_(display, ArrowDown) {}

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;
    auto checkAtEnd() -> bool;
    void render(DisplaySystem &display) override;
    void restoreRendererStream();

    void processCommand(RemoteCommand command) override;

    auto insetBoundingBox() -> DisplayRect;
    void resetBoundingBox(DisplayRect oldDisplayBoundingBox);
    void setPlacemark();
    void setFirstLoad() { firstLoad_ = true; }
    void setPlaceInReadable(std::shared_ptr<EPubPlace> placeInReadable);

protected:
    PreferencesStore &prefs_;
    FileSystem &fileSystem_;
    std::shared_ptr<Readable> readable_;
    Renderer &renderer_;
    DisplaySystem &display_;

    bool firstLoad_ = true;
    bool showUnread_ = false;
    std::shared_ptr<EPubPlace> placeInReadable_;
    std::shared_ptr<TxtPlace> newPlace_;
    std::shared_ptr<TxtPlace> prevPlace_;
    std::shared_ptr<TxtPlace> preservePlace_;
    std::shared_ptr<RendererStream> rendererStream_;
    bool atEnd_ = false;

    std::string upperLeftText_ = "Dismiss";
    std::string bottomRightText_;
    SingleLineTextView upperLeftTextView_;
    SingleLineTextView bottomRightTextView_;
    ArrowView leftArrowView_;
    ArrowView rightArrowView_;
};
