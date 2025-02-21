#pragma once

#include "Communications/SindarinAPI.hpp"
#include "EPub/EPubFile.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/BookQuestion.hpp"
#include "Renderers/Renderer.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"

class BookQuestionsScreen : public Screen, public EinkView {
public:
    enum class State { QUESTIONS_LIST, CONTEXTS_LIST, LOADING, ANSWER };

    BookQuestionsScreen(PreferencesStore &prefs, FileSystem &fileSystem,
                        const std::shared_ptr<Readable> &readable, Renderer &renderer,
                        const std::shared_ptr<RendererStream> &stream, DisplaySystem &display,
                        Telemetry &telemetry)
        : EinkView(display), display_(display), prefs_(prefs), fileSystem_(fileSystem),
          readable_(readable), renderer_(renderer), stream_(stream), telemetry_(telemetry),
          menuView_(display), backLabelView_(display, "Back", ArrowLeft), questionView_(display),
          descriptionView_(display, 6), backgroundView_(display, 5), state_(State::QUESTIONS_LIST) {
        menuView_.padding = 1;
        menuView_.itemsPerPage = 11;
        needsMenuItemsLoad_ = true;

        // Initialize renderer place for answer view
        answerPlace_ = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));
    }

    void addMenuItem(const std::shared_ptr<MenuItem> &item) { menuView_.addMenuItem(item); }

    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void loadMenuItems();
    void loadQuestions();
    void loadContexts(const BookQuestion &question);

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
    BoxView backgroundView_;

    BookQuestions questions_;
    BookQuestion selectedQuestion_;
    BookQuestionResponse questionResponse_;
    std::shared_ptr<EPubPlace> placeInReadable_;

    bool needsMenuItemsLoad_{true};
    size_t selectedQuestionIndex_{0};
    State state_{State::QUESTIONS_LIST};

    std::shared_ptr<TxtPlace> answerPlace_;
    std::shared_ptr<RendererStream> rendererStream_;

    void moveSelection(int delta);
    void executeQuestion();
    void executeQuestion(const BookQuestionContext &context);
    void setState(State newState);
    void updateUIForState();
    void updateBackLabel();
};
