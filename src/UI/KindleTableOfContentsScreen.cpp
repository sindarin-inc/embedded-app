#include "KindleTableOfContentsScreen.hpp"

#include <ArduinoJson.h>
#include <algorithm>
#include <climits>
#include <cmath>

#include "Communications/SindarinAPI.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "UI/Menu/MenuItemTOC.hpp"
#include "sindarin-debug.h"

KindleTableOfContentsScreen::KindleTableOfContentsScreen(
    DisplaySystem &display, const std::shared_ptr<KindleBook> &kindleBook, PreferencesStore &prefs,
    Telemetry &telemetry, KindlePageFlipper &pageFlipper, int currentPage,
    const std::shared_ptr<CancelFlag> &cancelFlag)
    : EinkView(display), kindleBook_(kindleBook), telemetry_(telemetry), pageFlipper_(pageFlipper),
      currentPage_(currentPage), cancelFlag_(cancelFlag), backgroundView_(display, 5),
      menuView_(display), backLabelView_(display, "Back"), loadingMessageView_(display, 5),
      errorMessageView_(display, 5) {

    menuView_.padding = 1;
    menuView_.itemsPerPage = 11;

    loadingMessageView_.setText("Loading table of contents...");
    loadingMessageView_.alignCenter = true;
    loadingMessageView_.dynamicWidth = false;

    errorMessageView_.setText("");
    errorMessageView_.alignCenter = true;
}

void KindleTableOfContentsScreen::begin() {
    setState(State::LOADING);
    loadTableOfContents();
}

void KindleTableOfContentsScreen::setState(State newState) {
    state_ = newState;
    setNeedsLayout();
    setNeedsRerender();
}

void KindleTableOfContentsScreen::loadTableOfContents() {
    log_i("Loading table of contents for book: %s", kindleBook_->title.c_str());

    // Log telemetry for opening table of contents
    telemetry_.logMetric(TelemetryField::METRIC_EVENT, kindleBook_->uuid,
                         {{"event", std::string("table_of_contents_opened")}, {"kindle", true}});

    DispatchGetCore0BackgroundQueue()->dispatch([this]() {
        if (CancelFlag::isCancelled(cancelFlag_)) {
            return;
        }

        // Make network request for table of contents
        auto api = SindarinAPI::getInstance();

        // Use BLE if that's what we're currently using
        bool isUsingBle = pageFlipper_.isUsingBleConnection();

        // Use POST with empty chapter_uuid to get chapters WITH UUIDs
        auto response = api->fetchKindleChaptersWithUuids(std::string(kindleBook_->uuid),
                                                          currentPage_, isUsingBle, cancelFlag_);

        if (CancelFlag::isCancelled(cancelFlag_)) {
            return;
        }

        DispatchGetUiQueue()->dispatch([this, response]() {
            if (response.empty()) {
                log_e("Failed to get table of contents");
                errorMessageView_.setText("Failed to load table of contents");
                setState(State::ERROR);
                render(display_);
            } else {
                handleTableOfContentsResponse(response);
            }
        });
    });
}

void KindleTableOfContentsScreen::handleTableOfContentsResponse(const std::string &response) {
    log_d("TOC Response: %s", response.c_str());

    // Most TOC payloads are a couple of kilobytes; size the document based on the
    // response length with a little structural headroom
    const size_t capacity = response.size() + JSON_OBJECT_SIZE(12) + JSON_ARRAY_SIZE(32) + 512;
    auto doc = std::make_shared<SpiRamJsonDocument>(capacity);
    DeserializationError error = deserializeJson(*doc, response);

    if (error) {
        log_e("Failed to parse table of contents response: %s", error.c_str());
        errorMessageView_.setText("Failed to parse table of contents");
        setState(State::ERROR);
        render(display_);
        return;
    }

    auto root = (*doc).as<JsonObject>();

    if (!root["success"] || !root["success"].as<bool>()) {
        const char *errorText = root["error"] | "Unknown error";
        log_e("Table of contents request failed: %s", errorText);
        errorMessageView_.setText(StringFormat("Error: %s", errorText));
        setState(State::ERROR);
        render(display_);
        return;
    }

    // Parse progress information if available (preferred source for current page)
    if (root.containsKey("progress") && root["progress"].is<JsonObject>()) {
        JsonObject progress = root["progress"];
        KindlePageProgress progressUpdate;
        progressUpdate.fromJson(progress);

        if (progressUpdate.hasCurrentPage) {
            currentPosition_ = progressUpdate.currentPage;
            log_d("Current progress position: page %d", currentPosition_);
        }

        pageFlipper_.updateProgressFromServer(progressUpdate);
    }

    // Use the book's reported progress when available; otherwise keep the caller-provided default
    currentPage_ = currentPosition_ >= 0 ? currentPosition_ : currentPage_;

    // Parse chapters
    if (!root.containsKey("chapters") || !root["chapters"].is<JsonArray>()) {
        log_e("No chapters array in response");
        errorMessageView_.setText("No chapters found");
        setState(State::ERROR);
        render(display_);
        return;
    }

    chapters_.clear();
    JsonArray chaptersArray = root["chapters"];

    for (JsonVariant value : chaptersArray) {
        ChapterInfo chapter;

        if (value.containsKey("title")) {
            chapter.title = value["title"].as<std::string>();
        }

        if (value.containsKey("uuid")) {
            chapter.uuid = value["uuid"].as<std::string>();
        }

        if (value.containsKey("page")) {
            chapter.page = value["page"];
        }

        if (!chapter.title.empty()) {
            log_d("Chapter: title='%s', uuid='%s', page=%d", chapter.title.c_str(),
                  chapter.uuid.c_str(), chapter.page);
            chapters_.push_back(chapter);
        }
    }

    log_d("Loaded %zu chapters", chapters_.size());

    if (chapters_.empty()) {
        errorMessageView_.setText("No chapters found");
        setState(State::ERROR);
        render(display_);
    } else {
        populateMenuItems();
        highlightCurrentChapter();
        // Don't use setHidden - just change state and let drawInRect handle what to draw
        setState(State::DISPLAYING);
        firstLoad_ = true; // Force full redraw to clear loading message
        render(display_);
    }
}

void KindleTableOfContentsScreen::populateMenuItems() {
    menuView_.resetMenuItems();

    for (const auto &chapter : chapters_) {
        auto chapterTitle = chapter.title; // Capture for lambda
        auto chapterUuid = chapter.uuid;

        // Use page number as second column, or empty string if no page
        std::string pageStr = chapter.page >= 0 ? Commify(chapter.page) : "";

        menuView_.addMenuItem(std::make_shared<MenuItemTOC>(chapter.title, pageStr,
                                                            [this, chapterTitle, chapterUuid]() {
            log_d("Selected chapter: %s (UUID: %s)", chapterTitle.c_str(), chapterUuid.c_str());
            selectChapter();
        }));
    }
}

void KindleTableOfContentsScreen::highlightCurrentChapter() {
    if (chapters_.empty()) {
        return;
    }

    int targetPage = currentPosition_ >= 0 ? currentPosition_ : currentPage_;
    if (targetPage < 0) {
        targetPage = 0;
    }

    int selectedIndex = -1;
    int selectedPage = -1;
    int nextIndex = -1;
    int nextPage = INT_MAX;

    for (size_t i = 0; i < chapters_.size(); ++i) {
        const auto &chapter = chapters_[i];
        if (chapter.page < 0) {
            continue;
        }

        if (chapter.page <= targetPage) {
            if (chapter.page >= selectedPage) {
                selectedIndex = static_cast<int>(i);
                selectedPage = chapter.page;
            }
        } else if (chapter.page < nextPage) {
            nextIndex = static_cast<int>(i);
            nextPage = chapter.page;
        }
    }

    if (selectedIndex == -1) {
        selectedIndex = (nextIndex != -1) ? nextIndex : 0;
    }

    menuView_.setSelectedIndex(static_cast<size_t>(selectedIndex));
}

void KindleTableOfContentsScreen::selectChapter() {
    if (menuView_.getSelectedIndex() >= 0 && menuView_.getSelectedIndex() < chapters_.size()) {
        const auto &chapter = chapters_[menuView_.getSelectedIndex()];

        log_d("Selected chapter index %zu: title='%s', uuid='%s', page=%d",
              menuView_.getSelectedIndex(), chapter.title.c_str(), chapter.uuid.c_str(),
              chapter.page);

        // Optimistically update progress based on the selected chapter's page
        if (chapter.page >= 0 && kindleBook_->progressTotalPages > 0) {
            KindlePageProgress progress;
            progress.currentPage = chapter.page;
            progress.hasCurrentPage = true;
            progress.totalPages = kindleBook_->progressTotalPages;
            progress.hasTotalPages = true;

            // Calculate percentage: (currentPage + 1) / totalPages * 100
            // Adding 1 because page indices are 0-based but we want 1-based for percentage
            int percentage =
                static_cast<int>(std::lround(((static_cast<double>(chapter.page) + 1.0) * 100.0) /
                                             static_cast<double>(kindleBook_->progressTotalPages)));
            progress.percentage = std::clamp(percentage, 0, 100);
            progress.hasPercentage = true;

            log_d("Optimistically updating progress to page %d of %d (%d%%)", chapter.page,
                  kindleBook_->progressTotalPages, progress.percentage);

            // Apply the progress update which will also save to disk
            pageFlipper_.updateProgressFromServer(progress);
        }

        int selectedPage = chapter.page;
        if (selectedPage < 0) {
            if (currentPosition_ >= 0) {
                selectedPage = currentPosition_;
            } else {
                selectedPage = currentPage_;
            }
        }

        if (selectedPage < 0) {
            selectedPage = 0;
        }

        // Log telemetry for chapter selection
        telemetry_.logMetric(TelemetryField::METRIC_EVENT, kindleBook_->uuid,
                             {{"event", std::string("table_of_contents_chapter_selected")},
                              {"kindle", true},
                              {"chapter", chapter.title},
                              {"page", selectedPage}});

        // Call the callback to navigate to the chapter with UUID
        if (chapterSelectedCallback_) {
            chapterSelectedCallback_(chapter.uuid, selectedPage);
        }

        // Dismiss this screen
        back();
    }
}

void KindleTableOfContentsScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        firstLoad_ = true;
    }
    setNeedsRerender();
}

auto KindleTableOfContentsScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    const auto inset = 2;
    auto fullFrame = SolRect::fromSize(size);

    // Always layout the background first to cover everything
    context.setViewFrame(backgroundView_, fullFrame);

    if (state_ == State::LOADING || state_ == State::ERROR) {
        // Center the loading or error message
        auto messageView = (state_ == State::LOADING) ? static_cast<View *>(&loadingMessageView_)
                                                      : static_cast<View *>(&errorMessageView_);

        context.setViewFrame(*messageView, SolRect::fromSize(size),
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);
        return size;
    }

    // Layout back button
    context.setViewFrame(backLabelView_, SolRect::fromSize(size),
                         LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

    // Layout menu view
    auto insetRect =
        context.setViewFrame(menuView_, SolRect::fromSize(size).inset(inset),
                             LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);

    return {size.width, insetRect.size.height + 2 * inset};
}

void KindleTableOfContentsScreen::drawInRect(SolRect rect) {
    // Always draw background first to clear any previous content
    backgroundView_.fillColor = display_.bg;
    backgroundView_.strokeColor = display_.bg; // No visible border
    backgroundView_.draw();

    if (state_ == State::LOADING) {
        loadingMessageView_.draw();
    } else if (state_ == State::ERROR) {
        errorMessageView_.draw();
    } else {
        backLabelView_.draw();
        menuView_.draw();
    }
}

void KindleTableOfContentsScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect());

    draw();

    if (firstLoad_ || state_ != State::DISPLAYING) {
        display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
        firstLoad_ = false;
    }

    if (state_ == State::DISPLAYING) {
        menuView_.drawFastElements();
        display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
    }
}

void KindleTableOfContentsScreen::processCommand(RemoteCommand command) {
    if (state_ != State::DISPLAYING) {
        if (command == RemoteCommand::BACK || command == RemoteCommand::LEFT) {
            back();
        }
        return;
    }

    switch (command) {
    case RemoteCommand::UP:
        moveSelection(-1);
        break;
    case RemoteCommand::DOWN:
        moveSelection(1);
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        selectChapter();
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        back();
        break;
    default:
        break;
    }
}
