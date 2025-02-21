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
    auto fullFrame = SolRect::fromSize(size);

    // Set up the background frame first
    auto backgroundFrame = fullFrame.inset(0);
    context.setViewFrame(backgroundView_, backgroundFrame);

    // Position back label at top inside the background
    auto contentFrame = backgroundFrame.inset(10, 10, 2, 3);
    auto backLabelRect = context.setViewFrame(backLabelView_, contentFrame,
                                              LayoutOptionsSizeToFit | LayoutOptionsAlignTop |
                                                  LayoutOptionsAlignLeft);

    if (state_ == State::QUESTIONS_LIST) {
        context.setViewFrame(menuView_, contentFrame.inset(inset),
                             LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);
        return size;
    } else {
        // Layout for contexts, answer, or loading screens
        // Start content below the back label with some padding
        auto questionRect = context.setViewFrame(
            questionView_, contentFrame.inset(inset, backLabelRect.bottom() + inset * 4),
            LayoutOptionsSizeToFitVertical);

        if (state_ == State::CONTEXTS_LIST) {
            auto descRect = context.setViewFrame(
                descriptionView_, contentFrame.inset(inset, questionRect.bottom() + inset * 2),
                LayoutOptionsSizeToFitVertical);

            context.setViewFrame(menuView_,
                                 contentFrame.inset(inset, descRect.bottom() + inset * 5),
                                 LayoutOptionsSizeToFitVertical);
            return size;
        } else if (state_ == State::ANSWER) {
            return size;
        } else if (state_ == State::LOADING) {
            // Center the loading text vertically in the content area
            auto loadingRect = contentFrame;
            auto verticalSpace = contentFrame.size.height - questionRect.size.height;
            loadingRect = loadingRect.inset(0, verticalSpace / 2);
            context.setViewFrame(questionView_, loadingRect,
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
            return size;
        }

        context.setViewFrame(descriptionView_,
                             contentFrame.inset(inset, questionRect.bottom() + inset * 2),
                             LayoutOptionsSizeToFitVertical);
        return size;
    }
}

void BookQuestionsScreen::drawInRect(SolRect rect) {
    // Always draw background first
    backgroundView_.fillColor = display_.bg;
    backgroundView_.strokeColor = display_.fg;
    backgroundView_.draw();

    // Then draw content
    backLabelView_.draw();

    if (state_ == State::CONTEXTS_LIST || state_ == State::LOADING) {
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

    if (state_ == State::ANSWER) {
        // Save the current renderer stream if needed
        if (rendererStream_ == nullptr) {
            rendererStream_ = renderer_.getStream();
        }

        // Create a stream with the answer text
        auto stream = std::make_shared<StdStringStream>(questionResponse_.answer, DocType::TXT);
        renderer_.setStream(stream);

        // Draw the background frame first
        draw();

        // Double up on the vertical insets like ReadableDictionaryScreen/ReadableSummaryScreen
        auto rect =
            DisplayRect::fromSizeAndInsets(display_.width(), display_.height(), {30, 30, 30, 30});
        rect = rect.inset(10, 10, 15, 15);
        renderer_.setRect(rect);

        // Render the page
        uint8_t darkPicturePercent = 0;
        std::shared_ptr<Place> newPlace;
        std::tie(newPlace, darkPicturePercent) = renderer_.renderPage(answerPlace_);
        answerPlace_ = std::dynamic_pointer_cast<TxtPlace>(newPlace);

        // Update display
        if (prefs_.fastTransitions.get().value_or(false)) {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        }
    } else {
        draw();

        if (firstLoad_) {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
            firstLoad_ = false;
        }

        // Only draw fast elements when in a state that shows the menu
        if (state_ == State::QUESTIONS_LIST || state_ == State::CONTEXTS_LIST) {
            menuView_.drawFastElements();
            display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
        }
    }
}

void BookQuestionsScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        if (state_ == State::QUESTIONS_LIST || state_ == State::CONTEXTS_LIST) {
            moveSelection(-1);
        } else if (state_ == State::LOADING || state_ == State::ANSWER) {
            // Restore renderer stream and exit to original text
            restoreRendererStream();
            setFirstLoad();
            setNeedsRerender();
            back();
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
        } else if (state_ == State::ANSWER) {
            // Page forward in answer
            setNeedsRerender();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        switch (state_) {
        case State::ANSWER:
            // Try to page backward in answer
            if (answerPlace_->getOffset() > 0) {
                answerPlace_ = std::dynamic_pointer_cast<TxtPlace>(renderer_.getPreviousPlace());
                setNeedsRerender();
            }
            break;
        case State::CONTEXTS_LIST:
            setState(State::QUESTIONS_LIST);
            menuView_.setSelectedIndex(selectedQuestionIndex_);
            break;
        default:
            restoreRendererStream();
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

    log_d("Executing question: slug=%s, context=%s", selectedQuestion_.slug.c_str(),
          context.context_type.c_str());
    log_d("Book UUID: %s", readable_->uuid.c_str());
    log_d("Chapter: %s, Offset: %u", placeInReadable_->getHRef().c_str(),
          placeInReadable_->getOffset());

    BookQuestionRequest request = {};
    request.question_slug = selectedQuestion_.slug;
    request.context_type = context.context_type;
    request.book_uuid = readable_->uuid;
    request.chapter = placeInReadable_->getHRef();
    request.chapter_offset = placeInReadable_->getOffset();

    auto resp = std::make_shared<BookQuestionResponse>();

    SindarinAPI::getInstance()->setAccessToken(prefs_.cloudToken.get().value());

    // Dispatch the API call to a background task
    DispatchQueue::dispatchOneOff(
        [this, request, resp]() {
        // Execute API call in background
        int result = SindarinAPI::getInstance()->askQuestion(request, resp.get(), false, nullptr);

        // Dispatch UI updates back to main thread
        DispatchGetUiQueue()->dispatch([this, result, resp]() {
            log_d("Question request result: %d (success=%d)", result, resp->success);

            if (result == 200 && resp->success) {
                log_d("Got successful response, answer length: %zu", resp->answer.length());
                questionResponse_ = *resp;
                setState(State::ANSWER);
            } else {
                log_e("Request failed: result=%d, success=%d, error='%s'", result, resp->success,
                      resp->error.c_str());
                questionResponse_.success = false;
                questionResponse_.error = "Failed to get answer from server";
                setState(State::ANSWER);
            }
        });
        },
        8192, 1, 1, "Question");

    setNeedsRerender();
}

void BookQuestionsScreen::setState(State newState) {
    state_ = newState;
    updateBackLabel();
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
        descriptionView_.setText("");
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

void BookQuestionsScreen::updateBackLabel() {
    if (state_ == State::LOADING || state_ == State::ANSWER) {
        backLabelView_.setText("Dismiss");
        backLabelView_.setArrowDirection(ArrowUp);
    } else {
        backLabelView_.setText("Back");
        backLabelView_.setArrowDirection(ArrowLeft);
    }
}

void BookQuestionsScreen::restoreRendererStream() {
    if (rendererStream_ != nullptr) {
        renderer_.setRect(GetTextRectFromPrefs(display_, prefs_));
        renderer_.setStream(rendererStream_);
        rendererStream_ = nullptr;
    }
}
