#pragma once

#include "Communications/SindarinAPI.hpp"
#include "EPub/EPubFile.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/BookQuestion.hpp"
#include "Renderers/Renderer.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/HorizontalPagingIndicatorView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"

class BookQuestionsScreen : public Screen, public EinkView {
public:
    enum class State { QUESTIONS_LIST, CONTEXTS_LIST, MODEL_LIST, LOADING, ANSWER };

    BookQuestionsScreen(PreferencesStore &prefs, FileSystem &fileSystem,
                        const std::shared_ptr<Readable> &readable, Renderer &renderer,
                        const std::shared_ptr<RendererStream> &stream, DisplaySystem &display,
                        Telemetry &telemetry)
        : EinkView(display), display_(display), prefs_(prefs), fileSystem_(fileSystem),
          readable_(readable), renderer_(renderer), stream_(stream), telemetry_(telemetry),
          menuView_(display), backLabelView_(display, "Back", ArrowLeft), questionView_(display),
          descriptionView_(display, 6), contextHeadingView_(display), backgroundView_(display, 5),
          pageIndicatorView_(display) {
        menuView_.padding = 1;
        menuView_.itemsPerPage = 11;

        // Initialize renderer place for answer view
        answerPlace_ = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));
    }

    void addMenuItem(const std::shared_ptr<MenuItem> &item) { menuView_.addMenuItem(item); }

    void setPlaceInReadable(std::shared_ptr<EPubPlace> placeInReadable) {
        placeInReadable_ = std::move(placeInReadable);
    }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void loadQuestions();
    void loadContexts(const BookQuestion &question);
    void reset();

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    FileSystem &fileSystem_;
    std::shared_ptr<Readable> readable_;
    Renderer &renderer_;
    std::shared_ptr<RendererStream> stream_;
    Telemetry &telemetry_;
    MenuView menuView_;

    ArrowLabelView backLabelView_;
    SingleLineTextView questionView_;
    MultiLineTextView descriptionView_;
    SingleLineTextView contextHeadingView_;
    BoxView backgroundView_;
    HorizontalPagingIndicatorView pageIndicatorView_;

    BookQuestions questions_;
    BookQuestion selectedQuestion_;
    BookQuestionResponse questionResponse_;
    std::shared_ptr<EPubPlace> placeInReadable_;
    SpiramString selectedModelId_;
    BookQuestionContext selectedContext_;

    bool needsMenuItemsLoad_{true};
    size_t selectedQuestionIndex_{0};
    size_t selectedContextIndex_{0};
    size_t selectedModelIndex_{0};
    State state_{State::QUESTIONS_LIST};
    std::shared_ptr<CancelFlag> cancelFlag_;
    bool dismissed_{false};

    std::shared_ptr<TxtPlace> answerPlace_;
    std::shared_ptr<RendererStream> rendererStream_;

    size_t precomputedTotalPages_{0};
    std::vector<uint32_t> pageBreakOffsets_;
    std::vector<size_t> pageNumbers_;
    size_t currentPage_{0};

    void moveSelection(int delta);
    void executeQuestion();
    void executeQuestion(const BookQuestionContext &context);
    void executeQuestion(const BookQuestionContext &context, const std::string &modelId);
    auto connectToNetwork(bool useBleOnly, std::string &errorMessage) -> bool;
    void sendQuestionRequest(const BookQuestionRequest &request, bool useBleOnly);
    void setState(State newState);
    void updateUIForState();
    void updateBackLabel();
    void restoreRendererStream();
    void setLoadingMessage(const std::string &message);
    void precomputeTotalPages();
    void disconnectNetworkConnections();
    void dismissDialog();
};
