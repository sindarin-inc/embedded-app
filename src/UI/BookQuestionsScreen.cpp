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

    if (!showingContexts_) {
        auto insetRect =
            context.setViewFrame(menuView_, SolRect::fromSize(size).inset(inset),
                                 LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);
        return {size.width, insetRect.size.height + 2 * inset};
    } else {
        // When showing contexts, position question below back label with large gap
        auto questionRect = context.setViewFrame(
            questionView_, SolRect::fromSize(size).inset(inset, backLabelRect.bottom() + inset * 4),
            LayoutOptionsSizeToFitVertical);

        // Position description closer to question
        auto descRect = context.setViewFrame(
            descriptionView_,
            SolRect::fromSize(size).inset(inset, questionRect.bottom() + inset * 2),
            LayoutOptionsSizeToFitVertical);

        // Position menu below description with extra large gap
        auto menuRect = context.setViewFrame(
            menuView_, SolRect::fromSize(size).inset(inset, descRect.bottom() + inset * 5),
            LayoutOptionsSizeToFitVertical);

        return {size.width, menuRect.bottom() + inset};
    }
}

void BookQuestionsScreen::drawInRect(SolRect rect) {
    backLabelView_.draw();
    if (showingContexts_) {
        questionView_.draw();
        descriptionView_.draw();
    }
    menuView_.draw();
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
        moveSelection(-1);
        break;
    case RemoteCommand::DOWN:
        moveSelection(1);
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        if (!showingContexts_) {
            auto selectedIndex = menuView_.getSelectedIndex();
            if (selectedIndex < questions_.questions.size()) {
                selectedQuestionIndex_ = selectedIndex;
                selectedQuestion_ = questions_.questions[selectedIndex];
                showingContexts_ = true;
                loadMenuItems();
                setFirstLoad();
                setNeedsRerender();
            }
        } else {
            // TODO: Execute question with selected context
            back();
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        if (showingContexts_) {
            showingContexts_ = false;
            loadMenuItems();
            menuView_.setSelectedIndex(selectedQuestionIndex_);
            setFirstLoad();
            setNeedsRerender();
        } else {
            setFirstLoad();
            setNeedsRerender();
            back();
        }
        return;
    default:
        break;
    }
}

void BookQuestionsScreen::loadMenuItems() {
    menuView_.resetMenuItems();

    if (!showingContexts_) {
        // Load questions if they haven't been loaded yet
        if (questions_.questions.empty()) {
            loadQuestions();
            if (questions_.questions.empty()) {
                // If questions are still empty after loading, there was an error
                return; // loadQuestions() will have called back() on error
            }
        }

        // Add menu items
        for (const auto &question : questions_.questions) {
            menuView_.addMenuItem(
                std::make_shared<MenuItemSingleLine>(std::string(question.question), []() {}));
        }
    } else {
        questionView_.setText(std::string(selectedQuestion_.question));
        descriptionView_.setText(std::string(selectedQuestion_.description));

        // Only show allowed contexts for this question
        for (const auto &context : questions_.contexts) {
            if (std::find(selectedQuestion_.allowed_contexts.begin(),
                          selectedQuestion_.allowed_contexts.end(),
                          context.context_type) != selectedQuestion_.allowed_contexts.end()) {
                menuView_.addMenuItem(std::make_shared<MenuItemSingleLine>(
                    std::string(context.description), []() {}));
            }
        }
    }
}

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
