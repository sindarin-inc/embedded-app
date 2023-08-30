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
    ReadableSummaryScreen(PreferencesStore &prefs, std::shared_ptr<Readable> readable,
                          DisplaySystem &display)
        : EinkView(display), prefs_(prefs), readable_(std::move(readable)), renderer_(display),
          display_(display), upperLeftTextView_(display, upperLeftText_),
          bottomRightTextView_(display, bottomRightText_), leftArrowView_(display, ArrowUp),
          rightArrowView_(display, ArrowDown) {}

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;
    auto checkAtEnd() -> bool;
    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    auto insetBoundingBox() -> DisplayRect;
    void resetBoundingBox(DisplayRect oldDisplayBoundingBox);
    void setPlacemark();
    void setFirstLoad() { firstLoad_ = true; }
    void setPlaceInReadable(unsigned long placeInReadable);

protected:
    PreferencesStore &prefs_;
    std::shared_ptr<Readable> readable_;
    Renderer renderer_;
    DisplaySystem &display_;

    bool firstLoad_ = true;
    bool showUnread_ = false;
    unsigned long placeInReadable_ = 0;
    unsigned long newPlace_ = 0;
    unsigned long prevPlace_ = 0;
    unsigned long preservePlace_ = 0;
    bool atEnd_ = false;

    std::string upperLeftText_ = "Dismiss";
    std::string bottomRightText_;
    SingleLineTextView upperLeftTextView_;
    SingleLineTextView bottomRightTextView_;
    ArrowView leftArrowView_;
    ArrowView rightArrowView_;
};
