#pragma once

#include "Misc/Telemetry.hpp"
#include "Models/BookQuestion.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Components/SingleLineTextView.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"

class BookQuestionsScreen : public Screen, public EinkView {
public:
    BookQuestionsScreen(PreferencesStore &prefs, FileSystem &fileSystem,
                        const std::shared_ptr<Readable> &readable,
                        const std::shared_ptr<RendererStream> &stream, DisplaySystem &display,
                        Telemetry &telemetry)
        : EinkView(display), display_(display), prefs_(prefs), fileSystem_(fileSystem),
          readable_(readable), stream_(stream), telemetry_(telemetry), menuView_(display),
          backLabelView_(display, "Back"), descriptionView_(display, 2) {
        backLabelView_.setFont(&fontFace0);
        descriptionView_.setFont(&fontFace0);
        menuView_.padding = 1;
        menuView_.itemsPerPage = 11;
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
    std::shared_ptr<RendererStream> stream_;
    Telemetry &telemetry_;
    MenuView menuView_;

    SingleLineTextView backLabelView_;
    MultiLineTextView descriptionView_;

    BookQuestions questions_;
    bool showingContexts_ = false;
    BookQuestion selectedQuestion_;

    void moveSelection(int delta);
    void executeQuestion();
};
