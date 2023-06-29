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
#include "UI/Layout/View.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class ReadableSummaryScreen : public Screen, public EinkView {
public:
    ReadableSummaryScreen(PreferencesStore &prefs, std::shared_ptr<Readable> readable,
                          DisplaySystem &display)
        : EinkView(display), prefs_(prefs), readable_(std::move(readable)), renderer_(renderer),
          display_(display), upperLeftTextView_(display, upperLeftText_), leftArrowView_(display) {}

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;

    void render(DisplaySystem &display) override;

    void processCommand(RemoteCommand command) override;

    void setPlacemark();
    void setFirstLoad() { firstLoad_ = true; }
    void setNearestChapterSelection(unsigned long placeInFile);

protected:
    PreferencesStore &prefs_;
    std::shared_ptr<Readable> readable_;
    Renderer renderer_;
    DisplaySystem &display_;
    bool firstLoad_ = true;

    std::string upperLeftText_ = "Dismiss";
    SingleLineTextView upperLeftTextView_;

    ArrowView leftArrowView_;
};
