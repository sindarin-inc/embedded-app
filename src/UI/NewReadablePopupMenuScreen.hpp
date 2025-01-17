#include <utility>

#include "Misc/Telemetry.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/LocalStore.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/LightingAdjustScreen.hpp"
#include "UI/LineMenuScreen.hpp"
#include "UI/Menu/CarouselMenuScreen.hpp"
#include "UI/ReadableDictionaryScreen.hpp"
#include "UI/ReadablePlacemarkHistoryScreen.hpp"
#include "UI/ReadableSeekScreen.hpp"
#include "UI/ReadableSummaryScreen.hpp"
#include "UI/ReadableTableOfContentsScreen.hpp"

class NewReadablePopupMenuScreen : public CarouselMenuScreen {

public:
    NewReadablePopupMenuScreen(DisplaySystem &display, PreferencesStore &prefs,
                               FileSystem &fileSystem, std::shared_ptr<Readable> readable,
                               LightingController &lightingController, Renderer &renderer,
                               Telemetry &telemetry, LocalStore &localStore,
                               std::shared_ptr<RendererStream> &stream)
        : CarouselMenuScreen(display), display_(display), prefs_(prefs),
          readable_(std::move(readable)), renderer_(renderer), popupMenuBackground_(display, 5),
          popupMenuForeground_(display, 5), footer1TextView_(display, ""),
          footer2TextView_(display, ""), footer3TextView_(display, ""), telemetry_(telemetry),
          localStore_(localStore), fileSystem_(fileSystem), stream_(stream),
          lineMenuScreen_(prefs, display) {

        menuView_.padding = 0;
        menuView_.itemsPerPage = 8;
        menuView_.indicatorWidth = 5;
        menuView_.indicatorPadding = 5;
        menuView_.setIndicatorType(MenuView::IndicatorType::Dot);
    };
    ~NewReadablePopupMenuScreen() override = default;

    void render(DisplaySystem &display) override;
    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    void setUpPopupMenu();

    void preselectIndexAt(int index);

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    std::shared_ptr<Readable> readable_;
    Renderer &renderer_;

    BoxView popupMenuBackground_;
    BoxView popupMenuForeground_;

    // Two lines of footer to put stuff like the clock on
    SingleLineTextView footer1TextView_;
    SingleLineTextView footer2TextView_;
    SingleLineTextView footer3TextView_;

    Telemetry &telemetry_;
    LocalStore &localStore_;
    FileSystem &fileSystem_;
    std::shared_ptr<RendererStream> &stream_;
    LineMenuScreen lineMenuScreen_;

    ChapterIdx currentChapterIdx_ = -1;
};
