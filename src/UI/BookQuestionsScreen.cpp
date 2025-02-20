#include "UI/BookQuestionsScreen.hpp"

#include "Misc/StringUtil.hpp"
#include "UI/Fonts/Fonts.hpp"
#include "sindarin-debug.h"

void BookQuestionsScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        setFirstLoad();
    }
    setNeedsRerender();
}

auto BookQuestionsScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    const auto inset = 2;

    // Position back label at top
    auto backLabelRect = context.setViewFrame(backLabelView_, SolRect::fromSize(size),
                                              LayoutOptionsSizeToFit | LayoutOptionsAlignTop |
                                                  LayoutOptionsAlignLeft);

    if (state_ == State::QUESTIONS_LIST) {
        auto insetRect =
            context.setViewFrame(menuView_, SolRect::fromSize(size).inset(inset),
                                 LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);
        return {size.width, insetRect.size.height + 2 * inset};
    } else {
        // Layout for contexts, answer, or loading screens
        auto questionRect = context.setViewFrame(
            questionView_, SolRect::fromSize(size).inset(inset, backLabelRect.bottom() + inset * 4),
            LayoutOptionsSizeToFitVertical);

        auto descRect = context.setViewFrame(
            descriptionView_,
            SolRect::fromSize(size).inset(inset, questionRect.bottom() + inset * 2),
            LayoutOptionsSizeToFitVertical);

        if (state_ == State::CONTEXTS_LIST) {
            auto menuRect = context.setViewFrame(
                menuView_, SolRect::fromSize(size).inset(inset, descRect.bottom() + inset * 5),
                LayoutOptionsSizeToFitVertical);
            return {size.width, menuRect.bottom() + inset};
        }

        return {size.width, descRect.bottom() + inset};
    }
}

void BookQuestionsScreen::drawInRect(SolRect rect) {
    backLabelView_.draw();

    if (state_ != State::QUESTIONS_LIST) {
        backgroundView_.draw();
        questionView_.draw();
        descriptionView_.draw();
    }

    if (state_ == State::QUESTIONS_LIST || state_ == State::CONTEXTS_LIST) {
        menuView_.draw();
    }
}

void BookQuestionsScreen::render(DisplaySystem &display) {
    if (needsMenuItemsLoad_) {
        loadMenuItems();
        needsMenuItemsLoad_ = false;
    }

    setNeedsLayout();
    setFrame(defaultFrameRect());
    draw();

    if (firstLoad_) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    menuView_.drawFastElements();
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
}

void BookQuestionsScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        if (state_ == State::QUESTIONS_LIST || state_ == State::CONTEXTS_LIST) {
            moveSelection(-1);
        }
        break;
    case RemoteCommand::DOWN:
        if (state_ == State::QUESTIONS_LIST || state_ == State::CONTEXTS_LIST) {
            moveSelection(1);
        }
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        if (state_ == State::QUESTIONS_LIST) {
            auto selectedIndex = menuView_.getSelectedIndex();
            if (selectedIndex < questions_.questions.size()) {
                selectedQuestionIndex_ = selectedIndex;
                selectedQuestion_ = questions_.questions[selectedIndex];
                setState(State::CONTEXTS_LIST);
            }
        } else if (state_ == State::CONTEXTS_LIST) {
            auto selectedIndex = menuView_.getSelectedIndex();
            if (selectedIndex < questions_.contexts.size()) {
                executeQuestion(questions_.contexts[selectedIndex]);
            }
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        switch (state_) {
        case State::ANSWER:
            setState(State::CONTEXTS_LIST);
            break;
        case State::CONTEXTS_LIST:
            setState(State::QUESTIONS_LIST);
            menuView_.setSelectedIndex(selectedQuestionIndex_);
            break;
        default:
            setFirstLoad();
            setNeedsRerender();
            back();
            break;
        }
        break;
    default:
        break;
    }
}

void BookQuestionsScreen::loadMenuItems() { updateUIForState(); }

void BookQuestionsScreen::loadQuestions() {
    // Load and parse questions JSON
    SFile questionsFile = fileSystem_.open("/book_questions.json", FILE_READ);
    if (!questionsFile) {
        log_e("Failed to open book questions file");
        back();
        return;
    }

    std::string jsonStr;
    while (questionsFile.available()) {
        jsonStr += (char)questionsFile.read();
    }
    questionsFile.close();

    questions_ = BookQuestions::loadFromJson(jsonStr);
}

void BookQuestionsScreen::executeQuestion(const BookQuestionContext &context) {
    setState(State::LOADING);

    // Get current place from the stream directly
    placeInReadable_ = std::dynamic_pointer_cast<EPubPlace>(renderer_.getCurrentPlace());
    if (!placeInReadable_) {
        log_e("Failed to get current place");
        back();
        return;
    }

    BookQuestionRequest request = {};
    request.question_slug = selectedQuestion_.slug;
    request.context_type = context.context_type;
    request.href = placeInReadable_->getHRef();
    request.byte_offset = placeInReadable_->getOffset();

    auto resp = std::make_shared<BookQuestionResponse>();

    SindarinAPI::getInstance()->setAccessToken(prefs_.cloudToken.get().value());

    // TODO: Consider moving this to a background task if needed
    int result = SindarinAPI::getInstance()->askQuestion(request, resp.get(), false, nullptr);

    if (result == 200 && resp->success) {
        questionResponse_ = *resp;
        setState(State::ANSWER);
    } else {
        questionResponse_.success = false;
        questionResponse_.error = "Failed to get answer from server";
        setState(State::ANSWER);
    }
}

void BookQuestionsScreen::setState(State newState) {
    state_ = newState;
    updateUIForState();
    setFirstLoad();
    setNeedsRerender();
}

void BookQuestionsScreen::updateUIForState() {
    menuView_.resetMenuItems();

    switch (state_) {
    case State::QUESTIONS_LIST:
        if (questions_.questions.empty()) {
            loadQuestions();
            if (questions_.questions.empty()) {
                return; // loadQuestions() will have called back() on error
            }
        }
        for (const auto &question : questions_.questions) {
            menuView_.addMenuItem(
                std::make_shared<MenuItemSingleLine>(std::string(question.question), []() {}));
        }
        break;

    case State::CONTEXTS_LIST:
        questionView_.setText(std::string(selectedQuestion_.question));
        descriptionView_.setText(std::string(selectedQuestion_.description));
        for (const auto &context : questions_.contexts) {
            if (std::find(selectedQuestion_.allowed_contexts.begin(),
                          selectedQuestion_.allowed_contexts.end(),
                          context.context_type) != selectedQuestion_.allowed_contexts.end()) {
                menuView_.addMenuItem(std::make_shared<MenuItemSingleLine>(
                    std::string(context.description), []() {}));
            }
        }
        break;

    case State::LOADING:
        questionView_.setText("Loading...");
        descriptionView_.setText("Please wait while we process your question...");
        break;

    case State::ANSWER:
        questionView_.setText(std::string(selectedQuestion_.question));
        if (questionResponse_.success) {
            descriptionView_.setText(std::string(questionResponse_.answer));
        } else {
            descriptionView_.setText(std::string(questionResponse_.error));
        }
        menuView_.addMenuItem(std::make_shared<MenuItemSingleLine>("Back", []() {}));
        break;
    }
}
