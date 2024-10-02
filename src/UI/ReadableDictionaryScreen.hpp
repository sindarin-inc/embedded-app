#pragma once

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Misc/SpiramAllocator.hpp"
#include "Models/Place.hpp"
#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Screen.hpp"
#include "sindarin-debug.h"

class ReadableDictionaryScreen : public Screen, public EinkView {
public:
    ReadableDictionaryScreen(PreferencesStore &prefs, std::shared_ptr<Readable> readable,
                             DisplaySystem &display, Renderer &renderer)
        : EinkView(display), prefs_(prefs), readable_(std::move(readable)), renderer_(renderer),
          display_(display), dismissLabelView_(display, "Dismiss", ArrowUp),
          backgroundView_(display, 5) {}

    auto layout(LayoutContext &context, Size size) -> Size override;
    void drawInRect(Rect rect) override;
    auto checkAtEnd() -> bool;
    void render(DisplaySystem &display) override;
    void restoreRendererStream();

    void processCommand(RemoteCommand command) override;

    void setPlacemark();
    void setFirstLoad() override {
        Screen::setFirstLoad();
        loadedChapterHref_ = "";
    }
    void setPlaceInReadable(std::shared_ptr<EPubPlace> placeInReadable);

protected:
    PreferencesStore &prefs_;
    std::shared_ptr<Readable> readable_;
    Renderer &renderer_;
    DisplaySystem &display_;

    SpiramString loadedChapterHref_;
    std::shared_ptr<EPubPlace> placeInReadable_ = nullptr;
    std::shared_ptr<TxtPlace> newPlace_ = nullptr;
    std::shared_ptr<TxtPlace> prevPlace_ = nullptr;
    std::shared_ptr<TxtPlace> preservePlace_ = nullptr;
    std::shared_ptr<RendererStream> rendererStream_;
    bool atEnd_ = false;

    ArrowLabelView dismissLabelView_;
    BoxView backgroundView_;
};
