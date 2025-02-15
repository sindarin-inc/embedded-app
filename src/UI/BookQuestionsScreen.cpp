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

    context.setViewFrame(backLabelView_, SolRect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    if (!showingContexts_) {
        auto insetRect =
            context.setViewFrame(menuView_, SolRect::fromSize(size).inset(inset),
                                 LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);
        return {size.width, insetRect.size.height + 2 * inset};
    } else {
        auto descRect = context.setViewFrame(
            descriptionView_, SolRect(0, backLabelView_.frame().bottom() + inset, size.width, 0),
            LayoutOptionsSizeToFitVertical);

        auto menuRect = context.setViewFrame(menuView_,
                                             SolRect(0, descRect.bottom() + inset, size.width,
                                                     size.height - descRect.bottom() - inset),
                                             LayoutOptionsSizeToFitVertical);

        return {size.width, menuRect.bottom() + inset};
    }
}

void BookQuestionsScreen::drawInRect(SolRect rect) {
    backLabelView_.draw();
    if (showingContexts_) {
        descriptionView_.draw();
    }
    menuView_.draw();
}

void BookQuestionsScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect());
    draw();

    if (needsFirstLoad()) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        clearFirstLoad();
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
        // Load and parse questions JSON
        SFile questionsFile = fileSystem_.open("/data/book_questions.json", FILE_READ);
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

        // Add menu items
        for (const auto &question : questions_.questions) {
            menuView_.addMenuItem(std::make_shared<MenuItemSingleLine>(question.question, []() {}));
        }
    } else {
        descriptionView_.setText(selectedQuestion_.description);

        // Only show allowed contexts for this question
        for (const auto &context : questions_.contexts) {
            if (std::find(selectedQuestion_.allowed_contexts.begin(),
                          selectedQuestion_.allowed_contexts.end(),
                          context.context_type) != selectedQuestion_.allowed_contexts.end()) {
                menuView_.addMenuItem(
                    std::make_shared<MenuItemSingleLine>(context.description, []() {}));
            }
        }
    }
}
