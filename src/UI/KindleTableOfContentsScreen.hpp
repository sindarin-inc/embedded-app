#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Misc/CancelFlag.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/KindleBook.hpp"
#include "Models/KindlePageFlipper.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Components/ArrowLabelView.hpp"
#include "UI/Components/BoxView.hpp"
#include "UI/Components/EinkView.hpp"
#include "UI/Components/MultiLineTextView.hpp"
#include "UI/Layout/View.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"
#include "UI/Menu/MenuView.hpp"
#include "UI/Screen.hpp"

class KindleTableOfContentsScreen : public Screen, public EinkView {
public:
    struct ChapterInfo {
        std::string title;
        std::string uuid;
        int page = -1; // Optional page number
    };

    enum class State { LOADING, DISPLAYING, NAVIGATING, ERROR };

    KindleTableOfContentsScreen(DisplaySystem &display,
                                const std::shared_ptr<KindleBook> &kindleBook,
                                PreferencesStore &prefs, Telemetry &telemetry,
                                KindlePageFlipper &pageFlipper, int currentPage,
                                const std::shared_ptr<CancelFlag> &cancelFlag);

    ~KindleTableOfContentsScreen() override = default;

    void begin();
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    auto layout(LayoutContext &context, SolSize size) -> SolSize override;
    void drawInRect(SolRect rect) override;

    // Callback for when a chapter is selected (passes chapter UUID)
    void setChapterSelectedCallback(
        const std::function<void(const std::string &chapterUuid, int currentPage)> &callback) {
        chapterSelectedCallback_ = callback;
    }

protected:
    void setState(State newState);
    void loadTableOfContents();
    void handleTableOfContentsResponse(const std::string &response);
    void populateMenuItems();
    void selectChapter();
    void moveSelection(int delta);

private:
    std::shared_ptr<KindleBook> kindleBook_;
    Telemetry &telemetry_;
    KindlePageFlipper &pageFlipper_;
    int currentPage_;
    std::shared_ptr<CancelFlag> cancelFlag_;

    State state_{State::LOADING};
    std::vector<ChapterInfo> chapters_;
    int currentPosition_ = -1;

    BoxView backgroundView_;
    MenuView menuView_;
    ArrowLabelView backLabelView_;
    MultiLineTextView loadingMessageView_;
    MultiLineTextView errorMessageView_;

    std::function<void(const std::string &chapterUuid, int currentPage)> chapterSelectedCallback_;
    bool firstLoad_{true};
};